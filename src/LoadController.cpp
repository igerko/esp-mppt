#include "LoadController.h"

#include "SolarMPPTMonitor.h"
#include "TimeService.h"

LoadController::LoadController() = default;

void LoadController::setup()
{
    Preferences prefs;
    prefs.begin(PREF_NAME, true); // open namespace in read-only mode
    nextLoadOn_  = prefs.getULong64("nextOn", 0);
    nextLoadOff_ = prefs.getULong64("nextOff", 0);
    prefs.end();

    DBG_PRINTF("[LoadController] Loaded nextLoadOn=%ld, nextLoadOff=%ld\n",
                  (long)nextLoadOn_, (long)nextLoadOff_);
}

void LoadController::updateConfig(const String &payload) {
    JsonDocument doc;
    Preferences prefs;

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

    DBG_PRINTF("[LoadController] Parsed nextLoadOn: %ld\n", (long)nextLoadOn);
    DBG_PRINTF("[LoadController] Parsed nextLoadOff: %ld\n", (long)nextLoadOff);

    nextLoadOn_  = nextLoadOn;
    nextLoadOff_ = nextLoadOff;

    prefs.begin(PREF_NAME, false);
    prefs.putULong64("nextOn", (uint64_t)nextLoadOn_);
    prefs.putULong64("nextOff", (uint64_t)nextLoadOff_);
    prefs.end();

    DBG_PRINTF("[LoadController] Saved nextLoadOn=%ld, nextLoadOff=%ld\n",
                  (long)nextLoadOn_, (long)nextLoadOff_);
}

bool isInWindow(time_t current, time_t on, time_t off) {
    if (on == 0 || off == 0) return false;

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
    if (currentTime == 0) return;

    int loadStatus;
    bool readStatus = SolarMPPTMonitor::readLoadState(loadStatus);

    DBG_PRINTF("[LoadController] setLoadBasedOnConfig() called\n");
    DBG_PRINTF("  currentTime = %lu\n", (unsigned long)currentTime);
    DBG_PRINTF("  nextLoadOn_ = %lu\n", (unsigned long)nextLoadOn_);
    DBG_PRINTF("  nextLoadOff_ = %lu\n", (unsigned long)nextLoadOff_);
    DBG_PRINTF("  readStatus = %s, loadStatus = %d\n", readStatus ? "true" : "false", loadStatus);

    if (!readStatus) {
        DBG_PRINTF("  -> Read status false, turning OFF\n");
        SolarMPPTMonitor::setLoad(false);
        return;
    }

    if (isInWindow(currentTime, nextLoadOn_, nextLoadOff_)) {
        DBG_PRINTF("  -> Conditions met, turning ON\n");
        SolarMPPTMonitor::setLoad(true);
    } else {
        DBG_PRINTF("  -> Conditions NOT met, turning OFF\n");
        SolarMPPTMonitor::setLoad(false);
    }
}
