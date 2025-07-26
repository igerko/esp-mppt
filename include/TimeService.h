#pragma once

#include <Preferences.h>
#include <esp_attr.h>

class TimeService {
public:
  static time_t getTimeUTC();
  static time_t getTimeInTZ();
  static void setESPTimeFromModem(const timeval &timeval);
  static void setTimeAfterWakeUp();
  static void debugTime();
  static time_t parseISO8601(const char *isoStr);
  static bool isTimeToUseModem();
  static void updateLastModemPreference();

private:
  static time_t myTimegm(tm *tm);
};

inline RTC_DATA_ATTR bool isTimeInitializedFromModem = false;
inline RTC_DATA_ATTR time_t storedEpoch = -1;