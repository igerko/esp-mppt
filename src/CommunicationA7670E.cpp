#ifdef TINY_GSM_MODEM_A7670
#include "CommunicationA7670E.h"

#include <HardwareSerial.h>
#include <Wire.h>
#include <base64.h>
#include <esp_task_wdt.h>

#include "Globals.h"
#include "LoadController.h"
#include "SolarMPPTMonitor.h"
#include "TimeService.h"
#include "secrets.h"
#include <Update.h>

void CommunicationA7670E::setupModemImpl() {
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  DBG_PRINTLN(F("[ComA7670E] SerialAT started"));

#ifdef BOARD_POWERON_PIN
  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);
  DBG_PRINTLN(F("[ComA7670E] Set BOARD_POWERON_PIN HIGH"));
#endif

#ifdef MODEM_RESET_PIN
  pinMode(MODEM_RESET_PIN, OUTPUT);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  delay(2600);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  DBG_PRINTLN(F("[ComA7670E] Modem reset sequence complete"));
#endif

  pinMode(MODEM_DTR_PIN, OUTPUT);
  digitalWrite(MODEM_DTR_PIN, LOW);
  DBG_PRINTLN(F("[ComA7670E] Set DTR LOW"));

  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  DBG_PRINTLN(F("[ComA7670E] Sent modem power-on sequence"));

  DBG_PRINT(F("[ComA7670E] Start modem"));
  int retry = 0;
  while (!modem.testAT(1000)) {
    DBG_PRINT(".");
    if (retry++ > 10) {
      digitalWrite(BOARD_PWRKEY_PIN, LOW);
      delay(100);
      digitalWrite(BOARD_PWRKEY_PIN, HIGH);
      delay(1000);
      digitalWrite(BOARD_PWRKEY_PIN, LOW);
      retry = 0;
      DBG_PRINTLN(F("[ComA7670E] Retrying modem start..."));
    }
  }
  DBG_PRINTLN("");

  SimStatus sim = SIM_ERROR;
  while (sim != SIM_READY) {
    sim = modem.getSimStatus();
    switch (sim) {
      case SIM_READY:
        DBG_PRINTLN(F("[ComA7670E] SIM card online"));
        break;
      case SIM_LOCKED:
        DBG_PRINTLN(F("[ComA7670E] SIM card locked, trying to unlock..."));
        modem.simUnlock(SIMCARD_PIN_CODE);
        break;
      default:
        DBG_PRINTLN(F("[ComA7670E] Waiting for SIM..."));
        break;
    }
    delay(100);
  }

  DBG_PRINTF("[ComA7670E] Set network apn : %s\n", NETWORK_APN);
  modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), NETWORK_APN, "\"");
  if (modem.waitResponse() != 1) {
    DBG_PRINTLN("[ComA7670E] Set network apn error !");
  }

  int16_t sq;
  DBG_PRINT(F("[ComA7670E] Waiting for network registration"));
  RegStatus status = REG_NO_RESULT;
  while (status == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) {
    status = modem.getRegistrationStatus();
    switch (status) {
      case REG_UNREGISTERED:
      case REG_SEARCHING:
        sq = modem.getSignalQuality();
        DBG_PRINTF("[ComA7670E] [%lu] Signal Quality: %d\n", millis() / 1000, sq);
        delay(1000);
        break;
      case REG_DENIED:
        DBG_PRINTLN(F("[ComA7670E] Network registration denied, check APN"));
        return;
      case REG_OK_HOME:
        DBG_PRINTLN(F("[ComA7670E] Online registration successful"));
        break;
      case REG_OK_ROAMING:
        DBG_PRINTLN(F("[ComA7670E] Registered, roaming mode"));
        break;
      default:
        DBG_PRINTF("[ComA7670E] Unknown registration status: %d\n", status);
        delay(1000);
        break;
    }
  }

  DBG_PRINTF("[ComA7670E] Final registration status: %d\n", status);

  String ueInfo;
  if (modem.getSystemInformation(ueInfo)) {
    DBG_PRINT(F("[ComA7670E] UE System Info: "));
    DBG_PRINTLN(ueInfo);
  }

  if (!modem.setNetworkActive()) {
    DBG_PRINTLN(F("[ComA7670E] Enable network failed!"));
  } else {
    DBG_PRINTLN(F("[ComA7670E] Network activated"));
  }

  String ipAddress = modem.getLocalIP();
  DBG_PRINT("[ComA7670E] Network IP:"); DBG_PRINTLN(ipAddress);
}

void CommunicationA7670E::sendMPPTPayload() {
  if (!isModemOn()) {
    DBG_PRINTLN("[ComA7670E] Modem is offline.");
    return;
  }

  File   original       = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_READ);
  String TEMP_FILE_NAME = String(MPPT_LOG_FILE_NAME) + ".tmp";
  if (!original) {
    DBG_PRINTLN("[ComA7670E]  File not found");
    return;
  }

  File tempFile = LittleFS.open(TEMP_FILE_NAME.c_str(), "w");
  if (!tempFile) {
    DBG_PRINTLN("[ComA7670E]  Could not create temp file");
    original.close();
    return;
  }

  if (clientTelegraf.connected())
    DBG_PRINTF("[ComA7670E] HTTP Client Connected to %s:%d\n", HTTP_TELEGRAF_SERVER, HTTP_TELEGRAF_PORT);

  size_t failedLines = 0;
  while (original.available()) {
    String line = original.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;


    DBG_PRINTF("[ComA7670E] Begin request:\n");
    clientTelegraf.beginRequest();
    clientTelegraf.post(HTTP_TELEGRAF_RESOURCE_MPPT);
    clientTelegraf.sendHeader("Content-Type", "application/json");
    String auth       = String(TELEGRAM_HTTP_USER) + ":" + TELEGRAM_HTTP_PASS;
    String authBase64 = base64::encode(auth);
    clientTelegraf.sendHeader("Authorization", "Basic " + authBase64);
    clientTelegraf.sendHeader("Content-Length", String(line.length()));
    clientTelegraf.endRequest();

    DBG_PRINTF("[ComA7670E] Sending HTTP request\n");
    clientTelegraf.print(line);
    int    status       = clientTelegraf.responseStatusCode();
    String responseBody = clientTelegraf.responseBody();
    clientTelegraf.stop();

    DBG_PRINTF("[ComA7670E] Sent one event, status: %d\n", status);
    if (status < 200 || status > 299) {
      failedLines += 1;
      DBG_PRINTLN(
          "[ComA7670E] Line not written correctly -> moving "
          "to .tmp");
      tempFile.println(line);
    }
    esp_task_wdt_reset();
  }
  original.close();
  tempFile.close();
  DBG_PRINTLN("[ComA7670E] Remove original file");
  LittleFS.remove(MPPT_LOG_FILE_NAME);

  // set failed lines count in Preferences
  Preferences prefs;
  prefs.begin(PREF_NAME, false);
  prefs.putUInt(FAILED_LINES_COUNT, failedLines);
  prefs.end();

  if (failedLines > 0) {
    DBG_PRINTF("[ComA7670E] Failed %d lines. Moving TMP to ORIGINAL.\n", failedLines);
    LittleFS.rename(TEMP_FILE_NAME, MPPT_LOG_FILE_NAME);
  } else
    LittleFS.remove(TEMP_FILE_NAME);
}

void CommunicationA7670E::downloadConfig() {
  DBG_PRINTLN("[ComA7670E] Starting downloadConfig()");

  if (!isModemOn()) {
    DBG_PRINTLN("[ComA7670E] Modem is offline.");
    return;
  }

  if (!modem.isNetworkConnected()) {
    DBG_PRINTLN("Modem is not connected to network!");
    return;
  }

  clientFastApi.get("/");

  int status = clientFastApi.responseStatusCode();
  DBG_PRINTF("[ComA7670E] HTTP status code: %d\n", status);
  if (status != 200) {
    DBG_PRINTLN("[ComA7670E] HTTP request failed");
    return;
  }

  String response = clientFastApi.responseBody();

  loadController.updateConfigAndTime(response);
  clientFastApi.stop();
}

void CommunicationA7670E::performOtaUpdate() {
  DBG_PRINTLN("[ComA7670E] Starting OTA update check...");

  if (!isModemOn() || !modem.isNetworkConnected()) {
    DBG_PRINTLN("[ComA7670E] Modem not ready.");
    return;
  }

  // Step 1: Získaj firmware.json
  clientFastApi.get("/firmware.json");
  if (clientFastApi.responseStatusCode() != 200) {
    DBG_PRINTLN("[ComA7670E] Failed to fetch firmware metadata");
    return;
  }

  String metaJson = clientFastApi.responseBody();
  clientFastApi.stop();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, metaJson);
  if (error) {
    DBG_PRINTLN("[ComA7670E] JSON parsing failed");
    return;
  }

  String version = doc["version"];
  if (version == MPPT_FIRMWARE_VERSION) {
    DBG_PRINTLN("[ComA7670E] Firmware is up to date.");
    return;
  }

  int totalSize = doc["total_size"];
  JsonArray parts = doc["parts"];
  if (!parts || parts.size() == 0) {
    DBG_PRINTLN("[ComA7670E] No firmware parts defined");
    return;
  }

  DBG_PRINTF("[ComA7670E] Total size: %d bytes, chunks: %d\n", totalSize, parts.size());

  if (!Update.begin(totalSize)) {
    DBG_PRINTF("[ComA7670E] Not enough space to begin OTA (%d bytes)\n", totalSize);
    return;
  }

  uint8_t buffer[1024];
  int totalWritten = 0;
  int chunkIndex = 0;
  int progress = 0;

  for (JsonObject part : parts) {
    String url = part["url"];
    int expectedSize = part["size"];
    DBG_PRINTF("[ComA7670E] [Chunk %d] URL: http://%s%s\n", chunkIndex, OTA_SERVER, url.c_str());

    String fullUrl = "http://" + String(OTA_SERVER) + url;
    modem.https_begin();
    if (!modem.https_set_url(fullUrl.c_str())) {
      DBG_PRINTLN("[ComA7670E] Failed to set chunk URL");
      modem.https_end();
      Update.abort();
      return;
    }

    size_t actualSize = 0;
    int httpCode = modem.https_get(&actualSize);
    DBG_PRINTF("[ComA7670E] [Chunk %d] HTTP status: %d, reported size: %d bytes\n", chunkIndex, httpCode, actualSize);

    if (httpCode != 200) {
      DBG_PRINTF("[ComA7670E] [Chunk %d] HTTPS GET failed: %d\n", chunkIndex, httpCode);
      modem.https_end();
      Update.abort();
      return;
    }

    if (actualSize != expectedSize) {
      DBG_PRINTF("[ComA7670E] [Chunk %d] Size mismatch: expected %d, got %d\n", chunkIndex, expectedSize, actualSize);
    }

    int readBytes = 0;
    int retries = 0;

    while (readBytes < expectedSize && retries < 10) {
      int len = modem.https_body(buffer, sizeof(buffer));
      if (len <= 0) {
        retries++;
        delay(300);
        continue;
      }

      retries = 0;  // reset retries on success
      int written = Update.write(buffer, len);
      if (written != len) {
        DBG_PRINTF("[ComA7670E] Write error: %d != %d\n", written, len);
        modem.https_end();
        Update.abort();
        return;
      }

      readBytes += written;
      totalWritten += written;

      int newProgress = (totalWritten * 100) / totalSize;
      if (newProgress - progress >= 5 || newProgress == 100) {
        progress = newProgress;
        DBG_PRINTF("[ComA7670E] OTA Progress: %d%%\n", progress);
      }

      esp_task_wdt_reset();
    }

    modem.https_end();

    if (readBytes != expectedSize) {
      DBG_PRINTF("[ComA7670E] [Chunk %d] Incomplete: %d / %d bytes\n", chunkIndex, readBytes, expectedSize);
      Update.abort();
      return;
    }

    chunkIndex++;
  }

  DBG_PRINTF("[ComA7670E] OTA written total: %d / %d bytes\n", totalWritten, totalSize);

  if (!Update.end() || !Update.isFinished()) {
    DBG_PRINTLN("[ComA7670E] OTA failed or incomplete");
    return;
  }

  DBG_PRINTLN("[ComA7670E] OTA update successful. Rebooting...");
  delay(1500);
  ESP.restart();
}

int CommunicationA7670E::getSignalStrengthPercentage() {
  if (modem.isNetworkConnected()) {
    const int rssi = modem.getSignalQuality();  // 0–31
    if (rssi == 99) {
      return -1;
    } else {
      return (rssi * 100) / 31;
    }
  }
  return -1;
}

void CommunicationA7670E::powerOffModemImpl() {
  modem.poweroff();
}

#endif