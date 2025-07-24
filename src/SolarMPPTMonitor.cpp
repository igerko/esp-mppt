#include "SolarMPPTMonitor.h"
#include "Globals.h"
#include <iterator>
#include "LoggingService.h"
#include "TimeService.h"
#include <time.h>

const int MAX_RETRIES = 3; // how many times to retry
const int RETRY_DELAY_MS = 50; // delay between retries (optional)

SolarMPPTMonitor::SolarMPPTMonitor()
{
}

void SolarMPPTMonitor::setupRS465()
{
    pinMode(RS485_DERE, OUTPUT);
    digitalWrite(RS485_DERE, LOW);
    RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD, RS485_TXD);
    node.begin(1, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
}

void SolarMPPTMonitor::preTransmission() { digitalWrite(RS485_DERE, HIGH); }
void SolarMPPTMonitor::postTransmission() { digitalWrite(RS485_DERE, LOW); }

bool SolarMPPTMonitor::readRegister(const RegisterInfo& reg, float& outValue)
{
    uint8_t count = (reg.type == REG_U32) ? 2 : 1;

    uint8_t result = node.readInputRegisters(reg.address, count);
    if (result == node.ku8MBSuccess)
    {
        if (reg.type == REG_U16)
        {
            uint16_t raw = node.getResponseBuffer(0);
            outValue = raw * reg.scale;
        }
        else
        {
            // REG_U32
            uint16_t low = node.getResponseBuffer(0);
            uint16_t high = node.getResponseBuffer(1);
            uint32_t combined = ((uint32_t)high << 16) | low;
            outValue = combined * reg.scale;
        }
        return true;
    }
    return false;
}

bool SolarMPPTMonitor::readHoldingRegister(uint16_t address, uint16_t& outValue)
{
    uint8_t result = node.readHoldingRegisters(address, 1); // funkcia 0x03
    if (result == node.ku8MBSuccess)
    {
        outValue = node.getResponseBuffer(0);
        return true;
    }
    return false;
}

bool SolarMPPTMonitor::writeHoldingRegister(uint16_t address, uint16_t value)
{
    // Use function 0x06 (write single register)
    uint8_t result = node.writeSingleRegister(address, value);
    if (result == node.ku8MBSuccess)
    {
        Serial.print("Register 0x");
        Serial.print(address, HEX);
        Serial.print(" set to ");
        Serial.println(value);
        return true;
    }
    Serial.print("Error while writing to 0x");
    Serial.print(address, HEX);
    Serial.print(": ");
    Serial.println(result);
    return false;
}

bool SolarMPPTMonitor::readLoadState(bool& isOn)
{
    // coil 0x0002 = Remote control of load
    uint8_t result = node.readCoils(0x0002, 1);
    if (result == node.ku8MBSuccess)
    {
        isOn = node.getResponseBuffer(0);
        Serial.print("Current state LOAD: ");
        Serial.println(isOn ? "ON ✅" : "OFF ❌");
        return true;
    }
    else
    {
        Serial.print("Error during read LOAD, code: ");
        Serial.println(result);
        return false;
    }
}

bool SolarMPPTMonitor::setLoad(bool enable)
{
    const uint8_t result = node.writeSingleCoil(0x0002, enable); // Coil 2 = Load control
    if (result == node.ku8MBSuccess)
    {
        Serial.print("LOAD ");
        Serial.println(enable ? "zapnutý ✅" : "vypnutý ❌");
        return true;
    }
    else
    {
        Serial.print("Chyba pri ");
        Serial.print(enable ? "zapínaní" : "vypínaní");
        Serial.print(" LOAD, kód: ");
        Serial.println(result);
        return false;
    }
}

void SolarMPPTMonitor::readAndPrintRegisters() // used for debug
{
    for (auto& r : mpptReadRegisters)
    {
        if (float value; readRegister(r, value))
        {
            Serial.print(r.name);
            Serial.print(": ");
            Serial.println(value);
        }
        else
        {
            Serial.print("Chyba pri čítaní ");
            Serial.println(r.name);
        }
    }
    Serial.println("-------------------------");

    if (uint16_t mode; readHoldingRegister(HR_LoadControlMode, mode))
    {
        Serial.print("Load Control Mode: ");
        Serial.println(mode);
    }
    else
    {
        Serial.println("Nepodarilo sa prečítať režim.");
    }

    // set load control mode to MANUAL
    writeHoldingRegister(HR_LoadControlMode, 0);

    bool loadOn;
    if (readLoadState(loadOn))
    {
        if (loadOn)
        {
            setLoad(false);
        }
        else
        {
            setLoad(true);
        }
    }
    Serial.println("--------------------------------------------------");
}

LogEntry SolarMPPTMonitor::readLogsFromMPPT()
{
    LogEntry logEntry(timeService.getTime());

    for (auto& r : mpptReadRegisters)
    {
        float value;
        bool success = false;

        for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt)
        {
            if (readRegister(r, value))
            {
                success = true;
                break;
            }
            Serial.printf("Read attempt %d failed for %s (0x%04X)\n",
                          attempt, r.name, r.address);
            delay(RETRY_DELAY_MS);
        }

        if (success)
        {
            logEntry.addValue(r.address, value);
        }
        else
        {
            Serial.print("❌ Unable to read after retries: ");
            Serial.println(r.name);
        }
    }
    Serial.println("Reading done");

    return logEntry;
}

bool SolarMPPTMonitor::setDatetimeInMPPT()
{
    if constexpr (DEBUG)
    {
        DateTimeFields rtc{};
        readDatetimeInMPPT(rtc);
        Serial.printf("RTC in MPPT: %02u-%02u-%02u %02u:%02u:%02u\n",
                      rtc.year, rtc.month, rtc.day,
                      rtc.hour, rtc.minute, rtc.second);
    }


    Serial.println("Sending time to MPPT");

    time_t datetime = TimeService::getTime();
    if (datetime == -1)
    {
        return false;
    }
    tm t;
    localtime_r(&datetime, &t);

    uint8_t year = (t.tm_year + 1900) - 2000; // convert to device's expected format
    uint8_t month = t.tm_mon + 1;
    uint8_t day = t.tm_mday;
    uint8_t hour = t.tm_hour;
    uint8_t minute = t.tm_min;
    uint8_t second = t.tm_sec;

    uint16_t regSecMin = (minute << 8) | second;
    uint16_t regHourDay = (day << 8) | hour;
    uint16_t regMonthYear = (year << 8) | month;

    node.clearTransmitBuffer();
    node.setTransmitBuffer(0, regSecMin);
    node.setTransmitBuffer(1, regHourDay);
    node.setTransmitBuffer(2, regMonthYear);

    // Write 3 registers starting from base address
    uint8_t result = node.writeMultipleRegisters(HR_RTC_SecondMinute, 3);

    if constexpr (DEBUG)
    {
        DateTimeFields rtc{};
        readDatetimeInMPPT(rtc);
        Serial.printf("RTC in MPPT: %02u-%02u-%02u %02u:%02u:%02u\n",
                      rtc.year, rtc.month, rtc.day,
                      rtc.hour, rtc.minute, rtc.second);
    }

    return (result == node.ku8MBSuccess);
}

bool SolarMPPTMonitor::readDatetimeInMPPT(DateTimeFields& dt)
{
    bool ok = true;
    uint16_t regSecMin;
    uint16_t regHourDay;
    uint16_t regMonthYear;

    ok &= readHoldingRegister(HR_RTC_SecondMinute, regSecMin);
    ok &= readHoldingRegister(HR_RTC_HourDay, regHourDay);
    ok &= readHoldingRegister(HR_RTC_MonthYear, regMonthYear);

    if (!ok) return false;
    dt.second = regSecMin & 0xFF;
    dt.minute = (regSecMin >> 8) & 0xFF;

    dt.hour = regHourDay & 0xFF;
    dt.day = (regHourDay >> 8) & 0xFF;

    dt.month = regMonthYear & 0xFF;
    dt.year = (regMonthYear >> 8) & 0xFF;

    return true;
}
