#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "CommunicationSIM800L.h"
#include "Globals.h"
#include "LoadController.h"
#include "LoggingService.h"
#include "SleepManager.h"
#include "SolarMPPTMonitor.h"
#include "TimeService.h"

SleepManager   sleepManager;
HardwareSerial RS485Serial(2);  // use UART2
ModbusMaster   node;            // single Modbus master

CommunicationSIM800L   sim800Instance;
ICommunicationService* communicationService = &sim800Instance;
TimeService            timeService;
LoadController         loadController;

void setup() {
  esp_task_wdt_init(300, true);  // 5 minutes
  esp_task_wdt_add(nullptr);
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();

  sleepManager.afterWakeUpSetup();
  Serial.begin(115200);
  delay(100);
  SolarMPPTMonitor::initOrResetRS485(false);
  LoggingService::setup();
  loadController.setup();

  if (TimeService::isTimeToUseModem() && !communicationService->isModemOn())
    communicationService->setupModem();
  esp_task_wdt_reset();
}

void loop() {
  esp_task_wdt_reset();
  if (communicationService->isModemOn()) {
    // update modem used ts before log is generated
    TimeService::updateLastModemPreference();
  }
  LoggingService::logMPPTEntryToFile(SolarMPPTMonitor::readLogsFromMPPT());

  if (communicationService->isModemOn()) {
    communicationService->downloadConfig();

    communicationService->sendMPPTPayload();
  }
  loadController.setLoadBasedOnConfig();
  esp_task_wdt_reset();
  sleepManager.activateDeepSleep();
}