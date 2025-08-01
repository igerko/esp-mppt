#pragma once
#ifdef TINY_GSM_MODEM_A7670

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#include "ICommunicationService.h"

class CommunicationA7670E final : public ICommunicationService {
 public:
  explicit CommunicationA7670E()
  : modem(SerialAT),
    tinyGsmClient(modem),
    clientTelegraf(tinyGsmClient, HTTP_TELEGRAF_SERVER, HTTP_TELEGRAF_PORT),
    clientFastApi(tinyGsmClient, HTTP_MPPT_SERVER, HTTP_MPPT_PORT)
    // debugger(SerialAT, Serial)
  {}

  CommunicationA7670E(const CommunicationA7670E&)            = delete;
  CommunicationA7670E& operator=(const CommunicationA7670E&) = delete;
  int                  getSignalStrengthPercentage() override;
  void                 powerOffModemImpl() override;

  void sendMPPTPayload() override;
  void downloadConfig() override;
  void performOtaUpdate() override;

 protected:
  void setupModemImpl() override;

 private:
  TinyGsm       modem;
  TinyGsmClient tinyGsmClient;
  HttpClient    clientTelegraf;
  HttpClient    clientFastApi;
  // StreamDebugger debugger;
};

#endif
