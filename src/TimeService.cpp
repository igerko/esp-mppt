#include "TimeService.h"
#include "SolarMPPTMonitor.h"

time_t TimeService::getTime()
{
    if (!isTimeInitializedFromModem)
        return -1;
    tm t;
    time_t now2;
    time(&now2);
    localtime_r(&now2, &t);
    return now2;
}

void TimeService::setESPTimeFromModem(const timeval& timeval)
{
    settimeofday(&timeval, nullptr);
    isTimeInitializedFromModem = true;
    bool result = SolarMPPTMonitor::setDatetimeInMPPT();
    if ( !result)
    {
        Serial.println("Set ESP time from modem to MPPT failed.");
    }

}

void TimeService::setTimeAfterWakeUp()
{
    if (!isTimeInitializedFromModem)
        return; // nothing to restore

    // storedEpoch must have been saved before deep sleep
    const time_t newEpoch = storedEpoch + DEEP_SLEEP_DURATION; // seconds

    timeval tv{};
    tv.tv_sec = newEpoch;
    tv.tv_usec = 0;

    settimeofday(&tv, nullptr); // update ESP32 system time
}

void TimeService::debugTime()
{
    const time_t now = getTime();
    tm t{};
    localtime_r(&now, &t);
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
              t.tm_year + 1900,
              t.tm_mon + 1,
              t.tm_mday,
              t.tm_hour,
              t.tm_min,
              t.tm_sec);
}
