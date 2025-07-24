// globals.h
#pragma once

#include <HardwareSerial.h>
#include <ModbusMaster.h>

extern HardwareSerial RS485Serial;
extern ModbusMaster node;
class ICommunicationService;
extern ICommunicationService* communicationService;
class TimeService;
extern TimeService timeService;
class SleepManager;
extern SleepManager sleepManager;
class LoadController;
extern LoadController loadController;

#define SerialAT Serial1

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define DEEP_SLEEP_DURATION  60   /* Time ESP32 will go to sleep (in seconds) */

#define DEBUG true

#define MPPT_LOG_FILE_NAME "/mppt_log.log"
#define MY_ESP_DEVICE_ID "crss"

// GPRS APN, credentials, SIM PIN etc
#define APN "internet"
#define GPRS_USER ""
#define GPRS_PWD ""
#define GSM_PIN "0000"

// HTTP TODO: make it HTTPS
#define HTTP_SERVER "vps.igerko.com" // TODO Implement
#define HTTP_PORT 8081
#define HTTP_RESOURCE_MPPT "/crss"
#define HTTP_RESOURCE_LOAD_STATUS "/load_status"
#define HTTP_RESOURCE_READ_CONFIG "/read_config"

// RS485 pins
#define RS485_RXD 19
#define RS485_TXD 18
#define RS485_DERE 15
#define RS485_BAUD 115200

// Modem config - SIM800L
#define MODEM_RST             5
#define MODEM_PWRKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

#define MODEM_DTR            32
#define MODEM_RI             33

#define I2C_SDA              21
#define I2C_SCL              22
#define LED_GPIO             13
#define LED_ON               HIGH
#define LED_OFF              LOW

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

#define SIM800L_IP5306_VERSION_20200811
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_USE_GPRS true