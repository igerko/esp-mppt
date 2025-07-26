#ifndef LOAD_CONTROLLER_H
#define LOAD_CONTROLLER_H

#include <Preferences.h>

class LoadController {
public:
  LoadController();
  void setup();
  void updateConfig(const String &payload);
  void setLoadBasedOnConfig() const;

  void setNextLoadOn(time_t t) { nextLoadOn_ = t; }
  void setNextLoadOff(time_t t) { nextLoadOff_ = t; }

private:
  time_t getNextLoadOnDatetime() const { return nextLoadOn_; }
  time_t getNextLoadOffDatetime() const { return nextLoadOff_; }

  time_t nextLoadOn_ = 0;
  time_t nextLoadOff_ = 0;

private:
  int i = 1;
};

#endif