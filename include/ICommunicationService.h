// IModem.h
#pragma once

#include "Globals.h"
#include "TimeService.h"
#include "TinyGsmClient.h"
#include "secrets.h"

class ICommunicationService {
 public:
  virtual ~ICommunicationService() = default;

  virtual int getSignalStrengthPercentage() = 0;

  virtual void downloadConfig()  = 0;
  virtual void sendMPPTPayload() = 0;
  virtual void performOtaUpdate() = 0;

  void setupModem() {
    setupModemImpl();
    isModemOn_ = true;
  }

  void powerOffModem() {
    if (isModemOn_)
      powerOffModemImpl();
    isModemOn_ = false;
  }

  [[nodiscard]] bool isModemOn() const { return isModemOn_; }

 protected:
  virtual void setupModemImpl()    = 0;
  virtual void powerOffModemImpl() = 0;

 private:
  bool isModemOn_ = false;
};
