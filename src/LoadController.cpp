#include "LoadController.h"

#include "SolarMPPTMonitor.h"
#include "TimeService.h"

LoadController::LoadController() {
    prefs.begin("sync", true); // open namespace in read-only mode
    nextLoadOn_  = prefs.getULong64("nextOn", 0);
    nextLoadOff_ = prefs.getULong64("nextOff", 0);
    prefs.end();

    Serial.printf("[SyncService] Loaded nextLoadOn=%ld, nextLoadOff=%ld\n",
                  (long)nextLoadOn_, (long)nextLoadOff_);
}

void LoadController::setupMQTT()
{
}

void LoadController::updateConfig(time_t loadOn, time_t loadOff) {
    nextLoadOn_  = loadOn;
    nextLoadOff_ = loadOff;

    prefs.begin("sync", false);
    prefs.putULong64("nextOn", (uint64_t)nextLoadOn_);
    prefs.putULong64("nextOff", (uint64_t)nextLoadOff_);
    prefs.end();

    Serial.printf("[SyncService] Saved nextLoadOn=%ld, nextLoadOff=%ld\n",
                  (long)nextLoadOn_, (long)nextLoadOff_);
}

void LoadController::setLoadBasedOnConfig() const
{
    if (nextLoadOn_ == 0 || nextLoadOff_ == 0)
    {
        // initial value - turn load off as we don't have any info
        SolarMPPTMonitor::setLoad(false);
        return;
    }
    time_t currentTime = timeService.getTimeUTC();
    if (currentTime > nextLoadOn_ && currentTime < nextLoadOff_)
    {
        SolarMPPTMonitor::setLoad(true);
        return;
    }
    SolarMPPTMonitor::setLoad(false);
}
