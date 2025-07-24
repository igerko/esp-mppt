#include "CommunicationSIM800L.h"

#include <HardwareSerial.h>

#include "Globals.h"
#include <Wire.h>

#include "TimeService.h"
#include <base64.h>
#include <esp_task_wdt.h>

#include "SolarMPPTMonitor.h"


void CommunicationSIM800L::setupModemImpl()
{
    // Start power management
    if (setupPMU() == false)
    {
        Serial.println("Setting power error");
    }

#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    while (!modem.testAT())
    {
        Serial.println("Waiting for modem...");
        delay(500);
    }

    Serial.println("Restarting modem...");
    if (!modem.restart())
    {
        Serial.println("Modem failed to restart");
    }
    else
    {
        Serial.println("Modem ready!");
    }
    Serial.print("Modem Info: ");
    Serial.println(modem.getModemInfo());

    if (GSM_PIN && modem.getSimStatus() != 3)
    {
        modem.simUnlock(GSM_PIN);
    }
    Serial.print("Waiting for network...");
    if (!modem.waitForNetwork())
    {
        Serial.println(" fail");
        delay(5000);
        return;
    }
    Serial.println(" success");
    if (modem.isNetworkConnected())
    {
        Serial.println("Network connected");
    }
    // GPRS connection parameters are usually set after network registration
    Serial.print(F("Connecting to "));
    Serial.print(APN);
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PWD))
    {
        Serial.println(" fail");
        delay(10000);
        return;
    }
    Serial.println(" success");
    if (modem.isGprsConnected())
    {
        Serial.println("GPRS connected");
    }
}


bool CommunicationSIM800L::ensureNetwork()
{
    // 1) Check if the modem responds to AT commands, retry up to 3 times
    int attempts = 0;
    bool atOK = false;

    while (attempts < 3) {
        Serial.println("Calling testAT");
        if (modem.testAT()) {
            atOK = true;
            break; // modem responded
        }
        Serial.printf("[NET] Modem not responding, attempt %d/3. Restarting...\n", attempts + 1);

        digitalWrite(MODEM_POWER_ON, LOW);
        delay(100);
        digitalWrite(MODEM_POWER_ON, HIGH);

        // Pull down PWRKEY for more than 1 second according to manual requirements
        digitalWrite(MODEM_PWRKEY, HIGH);
        delay(100);
        digitalWrite(MODEM_PWRKEY, LOW);
        delay(1000);
        digitalWrite(MODEM_PWRKEY, HIGH);
        modem.restart(); // try to recover
        delay(2000);     // small delay after restart
        attempts++;
    }
    if (!atOK) {
        Serial.println("[NET] Modem failed to respond after 3 attempts!");
        return false; // give up
    }
    Serial.println("AT OK");

    // 3) Check if GPRS is connected
    Serial.print("Is GPRS connected ? ... ");
    if (!modem.isGprsConnected()) {
        Serial.println("[NET] GPRS not connected, trying to connect...");
        if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PWD)) {
            Serial.println("[NET] GPRS connection failed.");
            return false;
        }
        Serial.println("[NET] GPRS connected.");
    }
    Serial.println("yes");

    // 4) Everything is ready
    return true;
}


std::optional<timeval> CommunicationSIM800L::getTimeFromModem()
{
    modem.sendAT("+CCLK?");
    if (modem.waitResponse(1000, "+CCLK:") == 1)
    {
        const String timeStr = modem.stream.readStringUntil('\r');

        int yy = 0, MM = 0, dd = 0, hh = 0, mi = 0, ss = 0, tz_quarters = 0;
        sscanf(timeStr.c_str(), " \"%2d/%2d/%2d,%2d:%2d:%2d%3d\"", // NOLINT(*-err34-c)
               &yy, &MM, &dd, &hh, &mi, &ss, &tz_quarters);

        tm t = {};
        t.tm_year = 2000 + yy - 1900;
        t.tm_mon = MM - 1;
        t.tm_mday = dd;
        t.tm_hour = hh;
        t.tm_min = mi;
        t.tm_sec = ss;
        t.tm_isdst = -1;
        time_t utcTime = mktime(&t); // mktime already converts local tm to UTC
        timeval now = {.tv_sec = utcTime, .tv_usec = 0};
        return now;
    }
    return std::nullopt;
}

void CommunicationSIM800L::sendMPPTPayload()
{
    if (!isModemOn())
    {
        Serial.println("Modem is offline.");
        return;
    }
    if (!ensureNetwork()) {
        Serial.println("❌ Cannot send data, network is not ready.");
        return; // handle offline scenario
    }
    File original = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_READ);
    String TEMP_FILE_NAME = String(MPPT_LOG_FILE_NAME) + ".tmp";
    if (!original)
    {
        Serial.println("[FS] File not found");
        return;
    }

    File tempFile = LittleFS.open(TEMP_FILE_NAME.c_str(), "w");
    if (!tempFile)
    {
        Serial.println("[FS] Could not create temp file");
        original.close();
        return;
    }

    size_t failedLines = 0;
    while (original.available())
    {
        String line = original.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        Serial.printf("Begin request:\n");
        httpClient.beginRequest();
        httpClient.post(HTTP_RESOURCE_MPPT);
        httpClient.sendHeader("Content-Type", "application/json");
        String auth = String(TELEGRAM_HTTP_USER) + ":" + TELEGRAM_HTTP_PASS;
        String authBase64 = base64::encode(auth);
        httpClient.sendHeader("Authorization", "Basic " + authBase64);
        httpClient.sendHeader("Content-Length", String(line.length()));
        httpClient.endRequest();

        Serial.printf("Sending HTTP request\n");
        httpClient.print(line);
        int status = httpClient.responseStatusCode();
        String responseBody = httpClient.responseBody();
        Serial.println("Response body: " + String(responseBody));
        httpClient.stop();

        Serial.printf("[HTTP] Sent one event, status: %d\n", status);
        if (status < 200 || status > 299)
        {
            failedLines += 1;
            Serial.println("[HTTP] Line not written correctly -> moving to .tmp");
            tempFile.println(line);
        }
        esp_task_wdt_reset();
    }
    original.close();
    tempFile.close();
    Serial.println("[FS] Remove original file");
    LittleFS.remove(MPPT_LOG_FILE_NAME);
    if (failedLines > 0)
    {
        Serial.printf("[FS] Failed %d lines. Moving TMP to ORIGINAL.\n", failedLines);
        LittleFS.rename(TEMP_FILE_NAME, MPPT_LOG_FILE_NAME);
    }
    else LittleFS.remove(TEMP_FILE_NAME);
}

void CommunicationSIM800L::sendLoadStatusPayload()
{
    if (!isModemOn())
    {
        Serial.println("Modem is offline.");
        return;
    }
    if (!ensureNetwork()) {
        Serial.println("❌ Cannot send data, network is not ready.");
        return; // handle offline scenario
    }

    bool loadStatus;
    int intLoadStatus = (SolarMPPTMonitor::readLoadState(loadStatus), loadStatus);
    JsonDocument doc;
    doc[AdditionalJSONKeys::TIMESTAMP] = TimeService::getTimeUTC();
    doc[AdditionalJSONKeys::DEVICE_ID] = MY_ESP_DEVICE_ID;
    doc[AdditionalJSONKeys::LOAD_STATUS] = intLoadStatus;
    String out;
    serializeJson(doc, out);


    // Send HTTP POST
    httpClient.beginRequest();
    httpClient.setTimeout(10000);  // 10 seconds
    httpClient.post(HTTP_RESOURCE_LOAD_STATUS); // e.g. "/loadstatus"
    httpClient.sendHeader("Content-Type", "application/json");

    // Basic Auth
    String auth = String(TELEGRAM_HTTP_USER) + ":" + TELEGRAM_HTTP_PASS;
    String authBase64 = base64::encode(auth);
    httpClient.sendHeader("Authorization", "Basic " + authBase64);

    // Content length must be actual payload length
    httpClient.sendHeader("Content-Length", String(out.length()));
    httpClient.endRequest();

    // Write body
    httpClient.print(out);

    // Done
    httpClient.stop();
}

bool CommunicationSIM800L::setupPMU()
{
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    Wire.write(0x37);
    return Wire.endTransmission() == 0;
}

int CommunicationSIM800L::getSignalStrengthPercentage()
{
    if (modem.isNetworkConnected())
    {
        const int rssi = modem.getSignalQuality(); // 0–31
        if (rssi == 99)
        {
            return -1;
        }
        else
        {
            return (rssi * 100) / 31;
        }
    }
    return -1;
}

void CommunicationSIM800L::powerOffModemImpl()
{
    modem.poweroff();
}
