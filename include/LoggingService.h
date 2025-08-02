#pragma once

#include <ArduinoJson.h>

#include "Globals.h"
#include "LittleFS.h"
#include "map"

namespace AdditionalJSONKeys {
constexpr auto TIMESTAMP        = "ts";
constexpr auto DEVICE_ID        = "device_id";
constexpr auto SIGNAL_STRENGTH  = "signal";
constexpr auto REGISTERS        = "registers";
constexpr auto TOTAL_WAKE_TIME  = "total_wake_time";
constexpr auto LOAD_STATUS      = "load_status";
constexpr auto MODEM_SYNC_TIME  = "modem_sync_time";
constexpr auto FIRMWARE_VERSION = "firmware_version";
}  // namespace AdditionalJSONKeys

class LogEntry {
 public:
  explicit LogEntry(time_t ts, int loadState);
  [[nodiscard]] String toJson() const;
  void                 addValue(uint16_t regAddr, float regVal);

 private:
  uint32_t                  ts;
  int                       loadState;
  std::map<uint16_t, float> values;
};

class LoggingService {
 public:
  LoggingService() = default;
  static void   setup();
  static size_t logMPPTEntryToFile(const LogEntry& log);
  static bool   clearLogFile();
};
