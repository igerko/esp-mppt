//
// Created by igerko on 7/23/25.
//
#include "Globals.h"
#include "ICommunicationService.h"
#include "SleepManager.h"

#include <Preferences.h>

constexpr auto KEY_TOTAL_AWAKE_TIME = "awake_time";
constexpr auto PREF_NAME = "sleep";
Preferences sleepPrefs;


SleepManager::SleepManager(): wakeStartMillis(millis())
{
}

void SleepManager::activateDeepSleep()
{
    Serial.print("Power off modem ...");
    communicationService->powerOffModem();
    Serial.println("done");

    wakenFromDeepSleep = true;

    // compute current session awake time
    const uint32_t sessionAwake = (millis() - wakeStartMillis) / 1000;


    totalAwakeTime += sessionAwake;

    // save updated total awake time to NVS
    sleepPrefs.begin(PREF_NAME, false); // read/write
    sleepPrefs.putULong64(KEY_TOTAL_AWAKE_TIME, totalAwakeTime);
    sleepPrefs.end();

    Serial.printf("[SleepManager] Saving total awake time: %llu seconds (this session: %lu)\n",
                  totalAwakeTime, sessionAwake);

    // set time in slow memory
    const time_t now = time(nullptr);
    tm t;
    localtime_r(&now, &t);
    storedEpoch = now;
    // put ESP to deep sleep
    Serial.println("[SleepManager] Going to deep sleep...");
    Serial.println("---------------------------");
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

/**
 * Handles basic events after wake-up
 */
void SleepManager::afterWakeUpSetup()
{
    wakeStartMillis = millis();
    // Load previous total awake time from NVS
    sleepPrefs.begin(PREF_NAME, true); // read-only
    totalAwakeTime = sleepPrefs.getULong64(KEY_TOTAL_AWAKE_TIME, 0);
    sleepPrefs.end();

    if (!wakenFromDeepSleep)
        return;
    timeService.setTimeAfterWakeUp();

    Serial.println("[SleepManager] afterWakeUpSetup done.");
}

uint64_t SleepManager::getTotalWakeTime() const
{
    return totalAwakeTime;
}
