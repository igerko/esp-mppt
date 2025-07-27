#include "LoadController.h"

#include "SolarMPPTMonitor.h"
#include "TimeService.h"

LoadController::LoadController() = default;

void LoadController::setup() {
  Preferences prefs;
  prefs.begin(PREF_NAME, true);  // open namespace in read-only mode
  nextLoadOn_  = prefs.getULong64("nextOn", 0);
  nextLoadOff_ = prefs.getULong64("nextOff", 0);
  prefs.end();

  DBG_PRINTF("[LoadController] Loaded nextLoadOn=%ld, nextLoadOff=%ld\n", (long) nextLoadOn_, (long) nextLoadOff_);
}

void LoadController::updateConfig(const String& payload) {
  JsonDocument doc;
  Preferences  prefs;

  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    DBG_PRINT(F("[LoadController] deserializeJson() failed: "));
    DBG_PRINTLN(error.f_str());
    return;
  }

  const char* nextLoadOnStr  = doc["nextLoadOn"];
  const char* nextLoadOffStr = doc["nextLoadOff"];

  DBG_PRINTF("[LoadController] Raw nextLoadOn: %s\n", nextLoadOnStr);
  DBG_PRINTF("[LoadController] Raw nextLoadOff: %s\n", nextLoadOffStr);

  time_t nextLoadOn  = TimeService::parseISO8601(nextLoadOnStr);
  time_t nextLoadOff = TimeService::parseISO8601(nextLoadOffStr);

  DBG_PRINTF("[LoadController] Parsed nextLoadOn: %ld\n", (long) nextLoadOn);
  DBG_PRINTF("[LoadController] Parsed nextLoadOff: %ld\n", (long) nextLoadOff);

  nextLoadOn_  = nextLoadOn;
  nextLoadOff_ = nextLoadOff;

  prefs.begin(PREF_NAME, false);
  prefs.putULong64("nextOn", (uint64_t) nextLoadOn_);
  prefs.putULong64("nextOff", (uint64_t) nextLoadOff_);
  prefs.end();

  DBG_PRINTF("[LoadController] Saved nextLoadOn=%ld, nextLoadOff=%ld\n", (long) nextLoadOn_, (long) nextLoadOff_);
}

bool isInWindow(time_t current, time_t on, time_t off) {
  if (on == 0 || off == 0)
    return false;

  if (on < off) {
    // same-day window
    return (current > on && current < off);
  } else {
    // overnight window
    return (current > on || current < off);
  }
}

void LoadController::setLoadBasedOnConfig() const {
  time_t currentTime = timeService.getTimeUTC();
  if (currentTime == 0)
    return;

  int   loadStatus;
  float batteryPercent, batteryTemp;

  bool readLoadStatus    = SolarMPPTMonitor::readLoadState(loadStatus);
  bool readBatteryStatus = SolarMPPTMonitor::readBatteryStatus(batteryPercent, batteryTemp);

  DBG_PRINTF("[LoadController] setLoadBasedOnConfig() called\n");
  DBG_PRINTF("  currentTime = %lu\n", (unsigned long) currentTime);
  DBG_PRINTF("  nextLoadOn_ = %lu\n", (unsigned long) nextLoadOn_);
  DBG_PRINTF("  nextLoadOff_ = %lu\n", (unsigned long) nextLoadOff_);
  DBG_PRINTF("  batteryPercent = %.1f %%\n", batteryPercent);
  DBG_PRINTF("  batteryTemp = %.2f °C\n", batteryTemp);
  DBG_PRINTF("  readStatus = %s, loadStatus = %d\n", readLoadStatus ? "true" : "false", loadStatus);

  auto setLoadIfChanged = [&](bool desiredState) {
    if (loadStatus < 0) {
      DBG_PRINTF("  -> loadStatus unknown (%d), forcing state %s\n", loadStatus, desiredState ? "ON" : "OFF");
      SolarMPPTMonitor::setLoad(desiredState);
      return;
    }

    bool currentState = (loadStatus != 0);
    if (currentState != desiredState) {
      DBG_PRINTF("  -> Changing load state to %s (current=%s)\n", desiredState ? "ON" : "OFF",
                 currentState ? "ON" : "OFF");
      SolarMPPTMonitor::setLoad(desiredState);
    } else {
      DBG_PRINTF("  -> Desired state = current state (%s), no change\n", desiredState ? "ON" : "OFF");
    }
  };

  if (readBatteryStatus) {
    float cutoffHigh;
    float cutoffLow;

    if (batteryTemp < 0.0f) {
      cutoffHigh = CUTOFF_HIGH_WINTER;
      cutoffLow  = CUTOFF_LOW_WINTER;
    } else {
      cutoffHigh = CUTOFF_HIGH_SUMMER;
      cutoffLow  = CUTOFF_LOW_SUMMER;
    }

    bool loadWasOn = (loadStatus != 0);
    DBG_PRINTF("  cutoffHigh = %.1f %% , cutoffLow = %.1f %% , loadWasOn = %s\n", cutoffHigh, cutoffLow,
               loadWasOn ? "true" : "false");

    if (loadWasOn) {
      if (batteryPercent < cutoffLow) {
        DBG_PRINTF("  -> Battery SOC %.1f%% < cutoffLow %.1f%%, turning OFF\n", batteryPercent, cutoffLow);
        setLoadIfChanged(false);
        return;
      }
    } else {
      if (batteryPercent < cutoffHigh) {
        DBG_PRINTF("  -> Battery SOC %.1f%% < cutoffHigh %.1f%%, keep OFF\n", batteryPercent, cutoffHigh);
        setLoadIfChanged(false);
        return;
      }
    }
  }

  if (!readLoadStatus) {
    DBG_PRINTF("  -> Read status false\n");
    setLoadIfChanged(false);
    return;
  }

  if (isInWindow(currentTime, nextLoadOn_, nextLoadOff_)) {
    DBG_PRINTF("  -> Conditions met\n");
    setLoadIfChanged(true);
  } else {
    DBG_PRINTF("  -> Conditions NOT met\n");
    setLoadIfChanged(false);
  }
}