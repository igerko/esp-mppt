#include "LoggingService.h"
#include "ICommunicationService.h"
#include "SleepManager.h"

LogEntry::LogEntry(const time_t ts)
{
    this->ts = ts;
}

String LogEntry::toJson() const
{
    {
        JsonDocument doc;
        doc[AdditionalJSONKeys::TIMESTAMP] = ts;
        doc[AdditionalJSONKeys::DEVICE_ID] = MY_ESP_DEVICE_ID;
        doc[AdditionalJSONKeys::SIGNAL_STRENGTH] = communicationService->getSignalStrengthPercentage();
        doc[AdditionalJSONKeys::TOTAL_WAKE_TIME] = sleepManager.getTotalWakeTime();

        const JsonObject vals = doc[AdditionalJSONKeys::REGISTERS].to<JsonObject>();
        for (const auto& [fst, snd] : values)
        {
            char keyHex[7]; // enough for "0xFFFF"
            sprintf(keyHex, "0x%04X", fst);
            vals[keyHex] = snd;
        }

        String out;
        serializeJson(doc, out);
        return out;
    }
}

void LogEntry::addValue(uint16_t regAddr, float regVal)
{
    this->values.insert(std::pair(regAddr, regVal));
}

void LoggingService::setup()
{
    if (!LittleFS.begin(true))
    {
        // `true` will format if mount fails
        Serial.println("LittleFS mount failed!");
        return;
    }
    Serial.println("LittleFS mounted.");
}

size_t LoggingService::logMPPTEntryToFile(const LogEntry& log)
{
    File f = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_APPEND);
    if (!f)
    {
        Serial.println("Failed to open log file for appending");
        return 0;
    }
    const size_t writeSize = f.println(log.toJson());
    f.close();
    return writeSize;
}

bool LoggingService::clearLogFile()
{
    Serial.println("Clearing log file.");
    return LittleFS.remove(MPPT_LOG_FILE_NAME);
}

char* LoggingService::readLogFile() // todo: delete ?
{
    if (File f = LittleFS.open(MPPT_LOG_FILE_NAME, FILE_READ); !f) {
        Serial.println("❌ Failed to open file");
        static char emptyStr[] = "";
        return emptyStr;
    } else {
        size_t size = f.size();
        char *buffer = new char[size + 1];
        f.readBytes(buffer, size);
        buffer[size] = '\0';
        f.close();
        return buffer;
    }
}
