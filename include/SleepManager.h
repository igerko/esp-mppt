//
// Created by igerko on 7/23/25.
//

#ifndef SLEEPMANAGER_H
#define SLEEPMANAGER_H
#include <esp_attr.h>

#include <cstdint>

class SleepManager {
public:
  SleepManager();
  void activateDeepSleep();
  void afterWakeUpSetup();
  uint64_t getTotalWakeTime() const;

private:
  uint64_t totalAwakeTime = 0;  // total across all sessions (seconds)
  uint32_t wakeStartMillis = 0; // millis at wake start
};

inline RTC_DATA_ATTR bool wakenFromDeepSleep = false;

#endif // SLEEPMANAGER_H
