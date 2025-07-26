#include "TimeService.h"

#include <time.h>

#include "SolarMPPTMonitor.h"

constexpr auto KEY_LAST_MODEM_USED_TIME = "l_usd_m";

time_t TimeService::getTimeUTC()
{
  if (!isTimeInitializedFromModem)
    return 0;
  return time(nullptr);
}

time_t TimeService::getTimeInTZ()
{
  if (!isTimeInitializedFromModem)
    return 0;
  tm t;
  time_t now2;
  time(&now2);
  localtime_r(&now2, &t);
  return now2;
}

void TimeService::setESPTimeFromModem(const timeval& timeval)
{
  DBG_PRINTF("[TimeService] UTC timestamp: %d\n", timeval.tv_sec);
  settimeofday(&timeval, nullptr);

  isTimeInitializedFromModem = true;
  bool result = SolarMPPTMonitor::setDatetimeInMPPT();
  if (!result)
  {
    DBG_PRINTLN("[TimeService] Set ESP time from modem to MPPT failed.");
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
  const time_t now = getTimeUTC();
  tm t{};
  localtime_r(&now, &t);
  DBG_PRINTF("%04d-%02d-%02d %02d:%02d:%02d\n", t.tm_year + 1900, t.tm_mon + 1,
             t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

time_t TimeService::myTimegm(struct tm* tm)
{
  // Days per month (not accounting leap year)
  static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int year = tm->tm_year + 1900;
  int month = tm->tm_mon; // 0-11
  int day = tm->tm_mday - 1; // 0-based

  // Count days for all previous years
  long days = 0;
  for (int y = 1970; y < year; y++)
  {
    days += 365;
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
      days++; // leap year
  }

  // Count days for months in current year
  for (int m = 0; m < month; m++)
  {
    days += mdays[m];
    if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
    {
      days++; // add leap day in Feb
    }
  }

  days += day;

  // Convert days, hours, minutes, seconds to epoch
  return (((((days * 24L) + tm->tm_hour) * 60L) + tm->tm_min) * 60L) +
    tm->tm_sec;
}


time_t TimeService::parseISO8601(const char* isoStr)
{
  struct tm tm = {};
  int year, month, day, hour, minute, second;

  // parse only up to seconds, ignore fractional part
  if (sscanf(isoStr, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour,
             &minute, &second) == 6)
  {
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

    // Convert broken-down UTC time to time_t
    // On ESP32 you can use timegm (or manually adjust)
    return myTimegm(&tm); // ⚡ works as UTC
  }
  return 0;
}

bool TimeService::isTimeToUseModem()
{
  Preferences prefs;
  prefs.begin(PREF_NAME, true);
  ulong lastModemUsedTime = prefs.getULong(KEY_LAST_MODEM_USED_TIME, -1);
  size_t failedLines = prefs.getUInt(FAILED_LINES_COUNT, 0);
  prefs.end();
  if (failedLines > 0)
  {
    DBG_PRINTF(
      "[TimeService] Failed Lines (count: %d) will be synced in this loop.\n",
      failedLines);
    // Treat as first run
    return true;
  }
  time_t nowEpoch = getTimeUTC();
  tm utc;
  gmtime_r(&nowEpoch, &utc);

  if (nowEpoch < 1577836800)
  {
    DBG_PRINTF("[TimeService] 🚀 Time not valid yet (epoch=%ld)\n", nowEpoch);
    // Treat as first run
    return true;
  }

  DBG_PRINTLN(F("[TimeService] ---- isTimeToUseModem ----"));
  DBG_PRINTF("[TimeService] Current UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
             utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
             utc.tm_min, utc.tm_sec);
  DBG_PRINTF("[TimeService] Last stored sent time: lastModemUsedTime=%lu\n",
             lastModemUsedTime);

  if ((nowEpoch - lastModemUsedTime) >= SEND_INTERVAL_SEC)
  {
    DBG_PRINTLN(F("[TimeService] ✅ Interval elapsed, will use modem."));
    return true;
  }
  DBG_PRINTLN(F("[TimeService] ⏳ Skip modem."));
  return false;
}

void TimeService::updateLastModemPreference()
{
  time_t nowEpoch = getTimeUTC();
  tm utc;
  gmtime_r(&nowEpoch, &utc);

  Preferences prefs;
  prefs.begin(PREF_NAME, false);
  prefs.putULong(KEY_LAST_MODEM_USED_TIME, nowEpoch);
  prefs.end();

  DBG_PRINTLN(F("[TimeService] ---- updateLastModemPreference ----"));
  DBG_PRINTF("[TimeService] Stored new value KEY_LAST_MODEM_USED_TIME: %lu",
             nowEpoch);
}
