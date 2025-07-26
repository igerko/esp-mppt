#include "LoggingService.h"

#include "ICommunicationService.h"
#include "SleepManager.h"

LogEntry::LogEntry(const time_t ts, const int loadState) {
  this->ts = ts;
  this->loadState = loadState;
}

String LogEntry::toJson() const {
  {
    JsonDocument doc;
    doc[AdditionalJSONKeys::TIMESTAMP] = ts;
    doc[AdditionalJSONKeys::DEVICE_ID] = MY_ESP_DEVICE_ID;
    doc[AdditionalJSONKeys::SIGNAL_STRENGTH] =
        communicationService->getSignalStrengthPercentage();
    doc[AdditionalJSONKeys::TOTAL_WAKE_TIME] = sleepManager.getTotalWakeTime();
    doc[AdditionalJSONKeys::LOAD_STATUS] = loadState;

    const JsonObject vals = doc[AdditionalJSONKeys::REGISTERS].to<JsonObject>();
    for (const auto &[fst, snd] : values) {
      char keyHex[7]; // enough for "0xFFFF"
      sprintf(keyHex, "0x%04X", fst);
      vals[keyHex] = snd;
    }

    String out;
    serializeJson(doc, out);
    return out;
  }
}

void LogEntry::addValue(uint16_t regAddr, float regVal) {
  this->values.insert(std::pair(regAddr, regVal));
}

void LoggingService::setup() {
  if (!LittleFS.begin(true)) {
    // `true` will format if mount fails
    DBG_PRINTLN("[LoggingService] LittleFS mount failed!");
    return;
  }
  DBG_PRINTLN("[LoggingService] LittleFS mounted.");
}

size_t LoggingService::logMPPTEntryToFile(const LogEntry &log) {
  File f = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_APPEND);
  if (!f) {
    DBG_PRINTLN("[LoggingService] Failed to open log file for appending");
    return 0;
  }
  const size_t writeSize = f.println(log.toJson());
  f.close();
  DBG_PRINTF("[LoggingService] Logged %zu bytes from MPPT\n", writeSize);
  return writeSize;
}

bool LoggingService::clearLogFile() {
  DBG_PRINTLN("[LoggingService] Clearing log file.");
  return LittleFS.remove(MPPT_LOG_FILE_NAME);
}
