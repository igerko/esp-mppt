// globals.h
#pragma once

#define DEBUG true  // set to 1 for debugging

#if DEBUG
#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINT2(x, y) Serial.print(x, y)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define DBG_PRINTLN(x) Serial.println(x)
#else
#define DBG_PRINT(x)
#define DBG_PRINT2(x, y)
#define DBG_PRINTF(...)
#define DBG_PRINTLN(x)
#endif

#include <HardwareSerial.h>
#include <ModbusMaster.h>

extern HardwareSerial RS485Serial;
extern ModbusMaster   node;
class ICommunicationService;
extern ICommunicationService* communicationService;
class TimeService;
extern TimeService timeService;
class SleepManager;
extern SleepManager sleepManager;
class LoadController;
extern LoadController loadController;

#define SerialAT Serial1

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define DEEP_SLEEP_DURATION 120   /* Time ESP32 will go to sleep (in seconds) */

#define SEND_INTERVAL_SEC (15 * 60) /* 15 mins */
#define CUTOFF_HIGH_WINTER 60.0f
#define CUTOFF_LOW_WINTER 50.0f
#define CUTOFF_HIGH_SUMMER 50.0f
#define CUTOFF_LOW_SUMMER 40.0f

#define MPPT_LOG_FILE_NAME "/mppt_log.log"
#define MY_ESP_DEVICE_ID "crss"
#define PREF_NAME "crss-pref"
#define FAILED_LINES_COUNT "failed_lines_c"

#define HTTP_TELEGRAF_SERVER "telegraf-mppt.igerko.com"
#define HTTP_TELEGRAF_RESOURCE_MPPT "/crss"
#define HTTP_TELEGRAF_PORT 80

#define HTTP_MPPT_SERVER "mppt.igerko.com"
#define HTTP_MPPT_RESOURCE "/"
#define HTTP_MPPT_PORT 80

#define OTA_SERVER "mppt.igerko.com"
#define OTA_PORT 80
#define OTA_PATH "/firmware.bin"

#ifdef LILYGO_SIM800L
// GPRS APN, credentials, SIM PIN etc
#define APN "internet"
#define GPRS_USER ""
#define GPRS_PWD ""
#define GSM_PIN "0000"

#define MODEM_RST 5
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

#define MODEM_DTR 32
#define MODEM_RI 33

#define I2C_SDA 21
#define I2C_SCL 22
#define LED_GPIO 13
#define LED_ON HIGH
#define LED_OFF LOW

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

#define SIM800L_IP5306_VERSION_20200811
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_USE_GPRS true

/ RS485 pins
#define RS485_RXD 19
#define RS485_TXD 18
#define RS485_DERE 15
#define RS485_BAUD 115200

#elif defined(TINY_GSM_MODEM_A7670)

// GPRS APN, credentials, SIM PIN etc
#define NETWORK_APN "internet"
#define GPRS_USER ""
#define GPRS_PWD ""
#define SIMCARD_PIN_CODE "0000"

#define MODEM_BAUDRATE (115200)
#define MODEM_DTR_PIN (25)
#define MODEM_TX_PIN (26)
#define MODEM_RX_PIN (27)
// The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY_PIN (4)
// The modem power switch must be set to HIGH for the modem to supply power.
#define BOARD_POWERON_PIN (12)
#define MODEM_RING_PIN (33)
#define MODEM_RESET_PIN (5)
#define BOARD_MISO_PIN (2)
#define BOARD_MOSI_PIN (15)
#define BOARD_SCK_PIN (14)
#define BOARD_SD_CS_PIN (13)
#define BOARD_BAT_ADC_PIN (35)
#define MODEM_RESET_LEVEL HIGH
#define SerialAT Serial1

#define MODEM_GPS_ENABLE_GPIO (-1)
#define MODEM_GPS_ENABLE_LEVEL (-1)

// It is only available in V1.4 version. In other versions, IO36 is not connected.
#define BOARD_SOLAR_ADC_PIN (36)

#define SerialRS485 Serial2

#define RS485_RXD 19
#define RS485_TXD 18
#define RS485_DERE 23
#define RS485_BAUD 115200

#endif