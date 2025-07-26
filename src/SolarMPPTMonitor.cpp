#include "SolarMPPTMonitor.h"

#include <time.h>

#include <iterator>

#include "Globals.h"
#include "LoggingService.h"
#include "TimeService.h"

constexpr int MAX_RETRIES = 3;     // how many times to retry
constexpr int RETRY_DELAY_MS = 50; // delay between retries (optional)

SolarMPPTMonitor::SolarMPPTMonitor() = default;

void SolarMPPTMonitor::setupRS465() {
  pinMode(RS485_DERE, OUTPUT);
  digitalWrite(RS485_DERE, LOW);
  RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD, RS485_TXD);
  node.begin(1, RS485Serial);
}

bool SolarMPPTMonitor::readRegister(const RegisterInfo &reg, float &outValue) {
  uint8_t count = (reg.type == REG_U32) ? 2 : 1;

  uint8_t result = node.readInputRegisters(reg.address, count);
  if (result == node.ku8MBSuccess) {
    if (reg.type == REG_U16) {
      uint16_t raw = node.getResponseBuffer(0);
      outValue = raw * reg.scale;
    } else {
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

bool SolarMPPTMonitor::readHoldingRegister(uint16_t address,
                                           uint16_t &outValue) {
  uint8_t result = node.readHoldingRegisters(address, 1); // funkcia 0x03
  if (result == node.ku8MBSuccess) {
    outValue = node.getResponseBuffer(0);
    return true;
  }
  return false;
}

bool SolarMPPTMonitor::writeHoldingRegister(uint16_t address, uint16_t value) {
  // Use function 0x06 (write single register)
  uint8_t result = node.writeSingleRegister(address, value);
  if (result == node.ku8MBSuccess) {
    DBG_PRINT("[SolarMPPTMonitor] Register 0x");
    DBG_PRINT2(address, HEX);
    DBG_PRINT(" set to ");
    DBG_PRINTLN(value);
    return true;
  }
  DBG_PRINT("[SolarMPPTMonitor] Error while writing to 0x");
  DBG_PRINT2(address, HEX);
  DBG_PRINT(": ");
  DBG_PRINTLN(result);
  return false;
}

bool SolarMPPTMonitor::readLoadState(int &loadState) {
  // coil 0x0002 = Remote control of load
  uint8_t result = node.readCoils(0x0002, 1);
  loadState = -1;
  if (result == node.ku8MBSuccess) {
    loadState = node.getResponseBuffer(0);
    DBG_PRINT("[SolarMPPTMonitor] Current LOAD state in MPPT: ");
    DBG_PRINTLN(loadState ? "ON ✅" : "OFF ❌");
    return true;
  } else {
    DBG_PRINT("[SolarMPPTMonitor] Error during read LOAD, code: ");
    DBG_PRINTLN(result);
    return false;
  }
}

bool SolarMPPTMonitor::setLoad(bool enable) {
  const uint8_t result =
      node.writeSingleCoil(0x0002, enable); // Coil 2 = Load control
  if (result == node.ku8MBSuccess) {
    DBG_PRINT("[SolarMPPTMonitor] LOAD set to MPPT: ");
    DBG_PRINTLN(enable ? "ON" : "OFF");
    return true;
  } else {
    DBG_PRINT("[SolarMPPTMonitor] Error during ");
    DBG_PRINT(enable ? "turning on" : "turning off");
    DBG_PRINT(" LOAD, code: ");
    DBG_PRINTLN(result);
    return false;
  }
}

LogEntry SolarMPPTMonitor::readLogsFromMPPT() {
  DBG_PRINTLN("[SolarMPPTMonitor] Reading from MPPT");
  int loadState;
  readLoadState(loadState);
  LogEntry logEntry(timeService.getTimeUTC(), loadState);

  for (auto &r : mpptReadRegisters) {
    float value;
    bool success = false;

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
      if (readRegister(r, value)) {
        success = true;
        break;
      }
      DBG_PRINTF("[SolarMPPTMonitor] Read attempt %d failed for %s (0x%04X)\n",
                 attempt, r.name, r.address);
      delay(RETRY_DELAY_MS);
    }

    if (success) {
      logEntry.addValue(r.address, value);
    } else {
      DBG_PRINT("[SolarMPPTMonitor] Unable to read after retries: ");
      DBG_PRINTLN(r.name);
    }
  }

  return logEntry;
}

bool SolarMPPTMonitor::setDatetimeInMPPT() {
  if constexpr (DEBUG) {
    DateTimeFields rtc{};
    readDatetimeInMPPT(rtc);
    DBG_PRINTF(
        "[SolarMPPTMonitor] RTC in MPPT: %02u-%02u-%02u %02u:%02u:%02u\n",
        rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second);
  }

  DBG_PRINTLN("[SolarMPPTMonitor] Sending time to MPPT");

  time_t datetime = TimeService::getTimeInTZ();
  if (datetime == -1) {
    return false;
  }
  tm t;
  localtime_r(&datetime, &t);

  uint8_t year =
      (t.tm_year + 1900) - 2000; // convert to device's expected format
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

  if constexpr (DEBUG) {
    DateTimeFields rtc{};
    readDatetimeInMPPT(rtc);
    DBG_PRINTF(
        "[SolarMPPTMonitor] RTC in MPPT: %02u-%02u-%02u %02u:%02u:%02u\n",
        rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second);
  }

  return (result == node.ku8MBSuccess);
}

bool SolarMPPTMonitor::readDatetimeInMPPT(DateTimeFields &dt) {
  bool ok = true;
  uint16_t regSecMin;
  uint16_t regHourDay;
  uint16_t regMonthYear;

  ok &= readHoldingRegister(HR_RTC_SecondMinute, regSecMin);
  ok &= readHoldingRegister(HR_RTC_HourDay, regHourDay);
  ok &= readHoldingRegister(HR_RTC_MonthYear, regMonthYear);

  if (!ok)
    return false;
  dt.second = regSecMin & 0xFF;
  dt.minute = (regSecMin >> 8) & 0xFF;

  dt.hour = regHourDay & 0xFF;
  dt.day = (regHourDay >> 8) & 0xFF;

  dt.month = regMonthYear & 0xFF;
  dt.year = (regMonthYear >> 8) & 0xFF;

  return true;
}

bool SolarMPPTMonitor::readBatteryStatus(float &socPercent, float &tempC) {
  bool allOk = true;

  // Battery SOC (0x311A), scale 1.0f
  {
    RegisterInfo regSoc = {0x311A, "Battery SOC (%)", 1.0f, REG_U16};
    float rawSoc = 0.0f;
    if (readRegister(regSoc, rawSoc)) {
      socPercent = rawSoc; // already scaled
    } else {
      allOk = false;
    }
  }

  // Battery Temperature (0x3110), scale 0.01f
  {
    RegisterInfo regTemp = {0x3110, "Remote Battery Temp (°C)", 0.01f, REG_U16};
    float rawTemp = 0.0f;
    if (readRegister(regTemp, rawTemp)) {
      tempC = rawTemp; // already scaled
    } else {
      allOk = false;
    }
  }

  return allOk; // true only if both succeeded
}
