#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "CommunicationSIM800L.h"
#include "SolarMPPTMonitor.h"
#include "LoggingService.h"
#include "TimeService.h"


#include "LoadController.h"

#include <TinyGsmClient.h>
#include "Globals.h"
#include "SleepManager.h"

SleepManager sleepManager;
HardwareSerial RS485Serial(2); // use UART2
ModbusMaster node; // single Modbus master

CommunicationSIM800L sim800Instance;
ICommunicationService* communicationService = &sim800Instance;
TimeService timeService;
LoadController loadController;

void setup()
{
    esp_task_wdt_init(300, true); // 5 minutes
    esp_task_wdt_add(nullptr);
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    sleepManager.afterWakeUpSetup();
    Serial.begin(115200);
    delay(100);
    SolarMPPTMonitor::setupRS465();
    LoggingService::setup();
    loadController.setup();

    if (TimeService::isTimeToUseModem() && !communicationService->isModemOn())
        communicationService->setupModem();
    esp_task_wdt_reset();
}

void loop()
{
    esp_task_wdt_reset();

    loadController.setLoadBasedOnConfig();
    LoggingService::logMPPTEntryToFile(SolarMPPTMonitor::readLogsFromMPPT());

    if (communicationService->isModemOn())
    {
        communicationService->downloadConfig();
        communicationService->sendMPPTPayload();
        TimeService::updateLastModemPreference();
        loadController.setLoadBasedOnConfig();
    }
    esp_task_wdt_reset();
    sleepManager.activateDeepSleep();
}
