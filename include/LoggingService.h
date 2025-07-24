#pragma once

#include "LittleFS.h"
#include "map"
#include <ArduinoJson.h>
#include "Globals.h"

namespace AdditionalJSONKeys
{
    constexpr auto TIMESTAMP = "ts";
    constexpr auto DEVICE_ID = "device_id";
    constexpr auto SIGNAL_STRENGTH = "signal";
    constexpr auto REGISTERS = "registers";
    constexpr auto TOTAL_WAKE_TIME = "total_wake_time";
}

class LogEntry
{
public:
    explicit LogEntry(time_t ts);
    [[nodiscard]] String toJson() const;
    void addValue(uint16_t regAddr, float regVal);

private:
    uint32_t ts;
    std::map<uint16_t, float> values;
};

class LoggingService
{
public:
    LoggingService() = default;
    static void setup();
    static size_t logMPPTEntryToFile(const LogEntry& log);
    static bool clearLogFile();
    static char *readLogFile();
};
