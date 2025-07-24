#include <Arduino.h>
#include <LittleFS.h>

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
    sleepManager.afterWakeUpSetup();
    Serial.begin(115200);
    delay(100);
    SolarMPPTMonitor::setupRS465();
    LoggingService::setup();
    // TODO: do not setup modem always
    // TODO: enable modem and http when:
    //      - syncing files
    //      - setting load
    //      - download config

    if (!wakenFromDeepSleep){
        communicationService->setupModem();
        // communicationService->mqttConnect();
    }

}

void loop()
{
    loadController.setLoadBasedOnConfig();

    const size_t loggedSize = LoggingService::logMPPTEntryToFile(SolarMPPTMonitor::readLogsFromMPPT());
    Serial.printf("Logged %zu\n", loggedSize);

    File f = LittleFS.open(MPPT_LOG_FILE_NAME, "r");
    while (f.available())
    {
        String line = f.readStringUntil('\n');
        Serial.println(line); // or send via MQTT
    }
    f.close();

    bool loadState;
    SolarMPPTMonitor::readLoadState(loadState);
    communicationService->sendMPPTPayload();
    // communicationService->mqttPublishLoadStatus(loadState);
    // communicationService->mqttPublishMPPTLogs();

    LoggingService::clearLogFile();

    sleepManager.activateDeepSleep();
}
