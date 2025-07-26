#pragma once

#include "ICommunicationService.h"

#define SIM800L_IP5306_VERSION_20200811
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_USE_GPRS true
#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>

class CommunicationSIM800L final : public ICommunicationService {
 public:
  explicit CommunicationSIM800L()
      : modem(Serial1),
        tinyGsmClient(modem),
        httpClientTelegraf(tinyGsmClient, HTTP_SERVER, HTTP_TELEGRAF_PORT),
        httpClientFastApi(tinyGsmClient, HTTP_SERVER, HTTP_API_PORT) {}

  CommunicationSIM800L(const CommunicationSIM800L&)             = delete;
  CommunicationSIM800L&  operator=(const CommunicationSIM800L&) = delete;
  int                    getSignalStrengthPercentage() override;
  void                   powerOffModemImpl() override;
  std::optional<timeval> getTimeFromModem() override;

  void sendMPPTPayload() override;
  void downloadConfig() override;

 protected:
  void setupModemImpl() override;
  bool ensureNetwork();

 private:
  static bool   setupPMU();
  TinyGsm       modem;
  TinyGsmClient tinyGsmClient;
  HttpClient    httpClientTelegraf;
  HttpClient    httpClientFastApi;
};
