#pragma once

#include <esp_attr.h>
#include <optional>

class TimeService
{
public:
    static time_t getTime();
    static void setESPTimeFromModem(const timeval& timeval);
    static void setTimeAfterWakeUp();
    static void debugTime();
};

inline RTC_DATA_ATTR bool isTimeInitializedFromModem = false;
inline RTC_DATA_ATTR time_t storedEpoch = -1;