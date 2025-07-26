#include "CommunicationSIM800L.h"

#include <HardwareSerial.h>
#include <Wire.h>
#include <base64.h>
#include <esp_task_wdt.h>

#include "Globals.h"
#include "LoadController.h"
#include "SolarMPPTMonitor.h"
#include "TimeService.h"

void CommunicationSIM800L::setupModemImpl() {
  // Start power management
  if (setupPMU() == false) {
    DBG_PRINTLN("[CommunicationSIM800L] Setting power error");
  }

  // Keep reset high
  pinMode(MODEM_RST, OUTPUT);
  digitalWrite(MODEM_RST, HIGH);
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
    DBG_PRINTLN("[CommunicationSIM800L] Waiting for modem...");
    delay(500);
  }

  DBG_PRINTLN("[CommunicationSIM800L] Restarting modem...");
  if (!modem.restart()) {
    DBG_PRINTLN("[CommunicationSIM800L] Modem failed to restart");
  } else {
    DBG_PRINTLN("[CommunicationSIM800L] Modem ready!");
  }
  DBG_PRINT("[CommunicationSIM800L] Modem Info: ");
  DBG_PRINTLN(modem.getModemInfo());

  if (GSM_PIN && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }
  DBG_PRINT("[CommunicationSIM800L] Waiting for network...");
  if (!modem.waitForNetwork()) {
    DBG_PRINTLN(" fail");
    delay(5000);
    return;
  }
  DBG_PRINTLN(" success");
  if (modem.isNetworkConnected()) {
    DBG_PRINTLN("[CommunicationSIM800L] Network connected");
  }
  // GPRS connection parameters are usually set after network registration
  DBG_PRINT(F("[CommunicationSIM800L] Connecting to "));
  DBG_PRINT(APN);
  if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PWD)) {
    DBG_PRINTLN(" fail");
    delay(10000);
    return;
  }
  DBG_PRINTLN(" success");
  if (modem.isGprsConnected()) {
    DBG_PRINTLN("[CommunicationSIM800L] GPRS connected");
  }
}

bool CommunicationSIM800L::ensureNetwork() {
  // 1) Check if the modem responds to AT commands, retry up to 3 times
  int attempts = 0;
  bool atOK = false;

  while (attempts < 3) {
    DBG_PRINTLN("[CommunicationSIM800L] Calling testAT");
    if (modem.testAT()) {
      atOK = true;
      break; // modem responded
    }
    setupModem();
    delay(2000); // small delay after restart
    attempts++;
  }
  if (!atOK) {
    DBG_PRINTLN(
        "[CommunicationSIM800L] Modem failed to respond after 3 attempts!");
    return false; // give up
  }
  DBG_PRINTLN("[CommunicationSIM800L] AT OK");

  // 3) Check if GPRS is connected
  DBG_PRINT("[CommunicationSIM800L] Is GPRS connected ? ... ");
  if (!modem.isGprsConnected()) {
    DBG_PRINTLN(
        "\n[CommunicationSIM800L]  GPRS not connected, trying to connect...");
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PWD)) {
      DBG_PRINTLN("[CommunicationSIM800L] GPRS connection failed.");
      return false;
    }
    DBG_PRINTLN("[CommunicationSIM800L] GPRS connected.");
  }
  DBG_PRINTLN("yes");

  // 4) Everything is ready
  return true;
}

std::optional<timeval> CommunicationSIM800L::getTimeFromModem() {
  modem.sendAT("+CCLK?");
  if (modem.waitResponse(1000, "+CCLK:") == 1) {
    const String timeStr = modem.stream.readStringUntil('\r');

    int yy = 0, MM = 0, dd = 0, hh = 0, mi = 0, ss = 0, tz_quarters = 0;
    sscanf(timeStr.c_str(),
           " \"%2d/%2d/%2d,%2d:%2d:%2d%3d\"", // NOLINT(*-err34-c)
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

void CommunicationSIM800L::sendMPPTPayload() {
  if (!isModemOn()) {
    DBG_PRINTLN("[CommunicationSIM800L] Modem is offline.");
    return;
  }

  File original = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_READ);
  String TEMP_FILE_NAME = String(MPPT_LOG_FILE_NAME) + ".tmp";
  if (!original) {
    DBG_PRINTLN("[CommunicationSIM800L]  File not found");
    return;
  }

  File tempFile = LittleFS.open(TEMP_FILE_NAME.c_str(), "w");
  if (!tempFile) {
    DBG_PRINTLN("[CommunicationSIM800L]  Could not create temp file");
    original.close();
    return;
  }

  httpClientTelegraf.connect(HTTP_SERVER, HTTP_TELEGRAF_PORT);
  if (httpClientTelegraf.connected())
    DBG_PRINTF("[CommunicationSIM800L] HTTP Client Connected to %s:%d\n",
               HTTP_SERVER, HTTP_TELEGRAF_PORT);

  size_t failedLines = 0;
  while (original.available()) {
    String line = original.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    ensureNetwork();

    DBG_PRINTF("[CommunicationSIM800L] Begin request:\n");
    httpClientTelegraf.beginRequest();
    httpClientTelegraf.post(HTTP_RESOURCE_MPPT);
    httpClientTelegraf.sendHeader("Content-Type", "application/json");
    String auth = String(TELEGRAM_HTTP_USER) + ":" + TELEGRAM_HTTP_PASS;
    String authBase64 = base64::encode(auth);
    httpClientTelegraf.sendHeader("Authorization", "Basic " + authBase64);
    httpClientTelegraf.sendHeader("Content-Length", String(line.length()));
    httpClientTelegraf.endRequest();

    DBG_PRINTF("[CommunicationSIM800L] Sending HTTP request\n");
    httpClientTelegraf.print(line);
    int status = httpClientTelegraf.responseStatusCode();
    String responseBody = httpClientTelegraf.responseBody();
    DBG_PRINTLN("[CommunicationSIM800L] Response body: " +
                String(responseBody));
    httpClientTelegraf.stop();

    DBG_PRINTF("[CommunicationSIM800L] Sent one event, status: %d\n", status);
    if (status < 200 || status > 299) {
      failedLines += 1;
      DBG_PRINTLN("[CommunicationSIM800L] Line not written correctly -> moving "
                  "to .tmp");
      tempFile.println(line);
    }
    esp_task_wdt_reset();
  }
  original.close();
  tempFile.close();
  DBG_PRINTLN("[CommunicationSIM800L] Remove original file");
  LittleFS.remove(MPPT_LOG_FILE_NAME);

  // set failed lines count in Preferences
  Preferences prefs;
  prefs.begin(PREF_NAME, false);
  prefs.putUInt(FAILED_LINES_COUNT, failedLines);
  prefs.end();

  if (failedLines > 0) {
    DBG_PRINTF(
        "[CommunicationSIM800L] Failed %d lines. Moving TMP to ORIGINAL.\n",
        failedLines);
    LittleFS.rename(TEMP_FILE_NAME, MPPT_LOG_FILE_NAME);
  } else
    LittleFS.remove(TEMP_FILE_NAME);
}

void CommunicationSIM800L::downloadConfig() {
  if (!isModemOn()) {
    DBG_PRINTLN("[CommunicationSIM800L] Modem is offline.");
    return;
  }
  if (!ensureNetwork()) {
    DBG_PRINTLN(
        "[CommunicationSIM800L] Cannot send data, network is not ready.");
    return; // handle offline scenario
  }
  httpClientFastApi.connect(HTTP_SERVER, HTTP_API_PORT);
  if (httpClientFastApi.connected())
    DBG_PRINTF("[CommunicationSIM800L] HTTP Client Connected to %s:%d\n",
               HTTP_SERVER, HTTP_TELEGRAF_PORT);

  httpClientFastApi.get(HTTP_API_RESOURCE_READ_CONFIG);
  String responseBody = httpClientTelegraf.responseBody();
  loadController.updateConfig(responseBody);
  httpClientFastApi.stop();
}

bool CommunicationSIM800L::setupPMU() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(0x37);
  return Wire.endTransmission() == 0;
}

int CommunicationSIM800L::getSignalStrengthPercentage() {
  if (modem.isNetworkConnected()) {
    const int rssi = modem.getSignalQuality(); // 0–31
    if (rssi == 99) {
      return -1;
    } else {
      return (rssi * 100) / 31;
    }
  }
  return -1;
}

void CommunicationSIM800L::powerOffModemImpl() { modem.poweroff(); }
