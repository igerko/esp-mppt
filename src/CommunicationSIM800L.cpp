#include "CommunicationSIM800L.h"

#include <HardwareSerial.h>

#include "Globals.h"
#include <Wire.h>

#include "TimeService.h"


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
    while (!modem.testAT()) {
        Serial.println("Waiting for modem...");
        delay(500);
    }

    Serial.println("Restarting modem...");
    if (!modem.restart()) {
        Serial.println("Modem failed to restart");
    } else {
        Serial.println("Modem ready!");
    }
    Serial.print("Modem Info: ");
    Serial.println(modem.getModemInfo());

    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
        modem.simUnlock(GSM_PIN);
    }
    Serial.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
        Serial.println(" fail");
        delay(5000);
        return;
    }
    Serial.println(" success");
    if (modem.isNetworkConnected()) {
        Serial.println("Network connected");
    }
    // GPRS connection parameters are usually set after network registration
    Serial.print(F("Connecting to "));
    Serial.print(APN);
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PWD)) {
        Serial.println(" fail");
        delay(10000);
        return;
    }
    Serial.println(" success");
    if (modem.isGprsConnected()) {
        Serial.println("GPRS connected");
    }
}


std::optional<timeval> CommunicationSIM800L::getTimeFromModem()
{
    modem.sendAT("+CCLK?");
    if (modem.waitResponse(1000, "+CCLK:") == 1)
    {
        const String timeStr = modem.stream.readStringUntil('\n');

        int yy = 0, MM = 0, dd = 0, hh = 0, mi = 0, ss = 0, tz = 0;
        sscanf(timeStr.c_str(), " \"%2d/%2d/%2d,%2d:%2d:%2d%2d\"", // NOLINT(*-err34-c)
               &yy, &MM, &dd, &hh, &mi, &ss, &tz);

        tm t = {};
        t.tm_year = 2000 + yy - 1900;
        t.tm_mon = MM - 1;
        t.tm_mday = dd;
        t.tm_hour = hh;
        t.tm_min = mi;
        t.tm_sec = ss;
        time_t now = mktime(&t);
        timeval tv = {.tv_sec = now};
        settimeofday(&tv, nullptr);

        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
        Serial.printf("ESP32 time synced! (%s)\n", buf);
        return tv;
    }
    return std::nullopt;
}

void CommunicationSIM800L::sendMPPTPayload() // TODO Implement
{
    if (!isModemOn())
    {
        Serial.println("Modem is offline.");
        return;
    }
    File file = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_READ);
    if (!file)
    {
        Serial.println("[FS] File not found");
        return;
    }

    const size_t totalSize = file.size();

    httpClient.beginRequest();
    httpClient.post(HTTP_RESOURCE_TEST_REQUEST);  // <-- your resource
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("X-Api-Key", "SOME_SECRET_KEY");   // <-- your custom header
    httpClient.sendHeader("Content-Length", String(totalSize));
    httpClient.endRequest();

    // ===== Stream in chunks =====
    const size_t CHUNK_SIZE = 512;   // tune this
    uint8_t buffer[CHUNK_SIZE];
    size_t bytesRead = 0;

    while (file.available()) {
        size_t len = file.read(buffer, CHUNK_SIZE);
        if (len == 0) break;

        httpClient.write(buffer, len);   // send this chunk
        bytesRead += len;

        Serial.print("[HTTP] Sent chunk, total sent: ");
        Serial.println(bytesRead);
    }

    // ===== Finish and read response =====

    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();
    Serial.print("[HTTP] Status: ");
    Serial.println(statusCode);
    Serial.print("[HTTP] Response: ");
    Serial.println(response);

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
    if (modem.isNetworkConnected()){
        const int rssi = modem.getSignalQuality(); // 0–31
        if (rssi == 99) {
            return -1;
        } else {
            return (rssi * 100) / 31;
        }
    }
    return -1;
}

void CommunicationSIM800L::powerOffModemImpl()
{
    modem.poweroff();
}
