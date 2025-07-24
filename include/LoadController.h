#ifndef LOAD_CONTROLLER_H
#define LOAD_CONTROLLER_H

#include <Preferences.h>
#include <PubSubClient.h>

class LoadController {
public:
    LoadController();

    void setupMQTT();

    void updateConfig(time_t loadOn, time_t loadOff);
    void setLoadBasedOnConfig() const;

private:
    time_t getNextLoadOnDatetime() const { return nextLoadOn_; }
    time_t getNextLoadOffDatetime() const { return nextLoadOff_; }

    time_t nextLoadOn_ = 0;
    time_t nextLoadOff_ = 0;
    Preferences prefs;
};

#endif