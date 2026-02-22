# esp-mppt

ESP32 firmware for a solar MPPT monitoring and load control system. Runs on a **LILYGO TTGO T-A7670G/E/SA R2** development board (ESP32 + A7670E 4G LTE modem) and communicates with a solar charge controller over RS485/Modbus.

Every wake cycle the firmware reads telemetry from the MPPT, logs it to flash, sends it to a remote server over 4G, downloads the latest load schedule, and then goes back to deep sleep.

---

## Table of Contents

- [Hardware](#hardware)
- [System Overview](#system-overview)
- [Wake Cycle](#wake-cycle)
- [Architecture](#architecture)
  - [Components](#components)
  - [Module Responsibilities](#module-responsibilities)
- [RS485 / Modbus Register Map](#rs485--modbus-register-map)
- [Load Control Logic](#load-control-logic)
- [OTA Firmware Updates](#ota-firmware-updates)
- [Configuration](#configuration)
  - [secrets.h](#secretsh)
  - [Build Flags](#build-flags)
  - [Key Constants (Globals.h)](#key-constants-globalsh)
- [Pin Assignment](#pin-assignment)
- [Building & Flashing](#building--flashing)
- [Dependencies](#dependencies)
- [File System](#file-system)
- [Deep Sleep & Time Keeping](#deep-sleep--time-keeping)

---

## Hardware

| Component | Details |
|---|---|
| MCU board | LILYGO TTGO T-A7670G/E/SA R2 |
| MCU | ESP32 (240 MHz, dual-core) |
| Modem | SIMCom A7670E — 4G LTE Cat-1 (no GPS, no SMS used) |
| MPPT controller | Renogy/EPever-compatible, RS485 Modbus RTU |
| RS485 adapter | DE/RE controlled half-duplex |
| Power | Battery-backed solar system (monitored by the MPPT itself) |

---

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        esp-mppt (ESP32)                         │
│                                                                 │
│  ┌──────────────┐   RS485/Modbus   ┌──────────────────────┐    │
│  │ SolarMPPT    │◄────────────────►│  MPPT Controller     │    │
│  │ Monitor      │                  │  (Renogy / EPever)   │    │
│  └──────┬───────┘                  └──────────────────────┘    │
│         │ LogEntry                                              │
│  ┌──────▼───────┐   LittleFS                                   │
│  │ Logging      │──────────────── /mppt_log.log                │
│  │ Service      │                                              │
│  └──────┬───────┘                                              │
│         │                                                       │
│  ┌──────▼───────┐   4G LTE (A7670E)                           │
│  │ Communication│──────────────────► Telegraf  (telemetry)     │
│  │ A7670E       │──────────────────► FastAPI   (config + OTA)  │
│  └──────────────┘                                              │
│                                                                 │
│  ┌──────────────┐                                              │
│  │ Load         │◄── schedule from API                        │
│  │ Controller   │──────────────────► Relay / Load output       │
│  └──────────────┘                                              │
└─────────────────────────────────────────────────────────────────┘
```

---

## Wake Cycle

The device spends most of its time in deep sleep (default **120 seconds**). Each wake cycle follows this sequence:

```
Boot / Wake-up
     │
     ▼
afterWakeUpSetup()          ← restore time from RTC memory, load NVS state
     │
     ▼
LoggingService::setup()     ← mount LittleFS
     │
     ▼
LoadController::setup()     ← load nextLoadOn / nextLoadOff from NVS
     │
     ▼
isTimeToUseModem()?
  ├─ YES ─► setupModem()
  │          downloadConfig()    ← GET /  → update load schedule + sync time
  │          performOtaUpdate()  ← GET /firmware.json → OTA if new version
  │
  ▼
setLoadBasedOnConfig()      ← turn relay ON or OFF
     │
     ▼
updateLastModemPreference() ← save modem-used timestamp to NVS
     │
     ▼
readLogsFromMPPT()          ← read all Modbus registers → LogEntry
     │
     ▼
logMPPTEntryToFile()        ← append JSON line to /mppt_log.log
     │
     ▼
sendMPPTPayload()?          ← POST each buffered line to Telegraf
     │
     ▼
setLoadBasedOnConfig()      ← re-check after data send
     │
     ▼
activateDeepSleep()         ← power off modem, save state, esp_deep_sleep_start()
```

The modem is only activated when `isTimeToUseModem()` returns `true`, which happens when:
- The configured send interval (`SEND_INTERVAL_SEC`, default 15 min) has elapsed, **or**
- There are previously failed log lines that need to be retried, **or**
- The system clock has not yet been synchronized (first boot / RTC lost).

---

## Architecture

### Components

```
include/
├── Globals.h                 ← macros, pin defs, extern declarations
├── ICommunicationService.h   ← abstract modem interface
├── CommunicationA7670E.h     ← A7670E 4G implementation
├── CommunicationSIM800L.h    ← SIM800L implementation (alternative HW)
├── SolarMPPTMonitor.h        ← Modbus register map + read/write helpers
├── LoggingService.h          ← LittleFS log file + JSON serialization
├── LoadController.h          ← relay scheduling logic
├── TimeService.h             ← time sync, ISO8601 parsing, NVS helpers
├── SleepManager.h            ← deep sleep + wake-up state restore
└── secrets.h                 ← credentials (not committed, see template)

src/
├── main.cpp                  ← setup() + loop()
├── SolarMPPTMonitor.cpp
├── LoggingService.cpp
├── LoadController.cpp
├── TimeService.cpp
├── SleepManager.cpp
├── CommunicationA7670E.cpp
└── CommunicationSIM800L.cpp
```

### Module Responsibilities

| Module | Responsibility |
|---|---|
| `ICommunicationService` | Abstract interface: `setupModem`, `powerOffModem`, `sendMPPTPayload`, `downloadConfig`, `performOtaUpdate`, `getSignalStrengthPercentage` |
| `CommunicationA7670E` | Concrete 4G implementation using TinyGSM + ArduinoHttpClient. Handles modem power sequence, GPRS registration, HTTP POST to Telegraf, HTTP GET config, chunked OTA download |
| `SolarMPPTMonitor` | Reads input registers (voltages, currents, power, temperatures, energy stats) and holding registers (RTC, load mode) from the MPPT over RS485 Modbus RTU. Also writes load coil and RTC |
| `LoggingService` | Appends JSON-encoded `LogEntry` objects to `/mppt_log.log` on LittleFS. Each line is one measurement snapshot |
| `LogEntry` | Holds a timestamp, load state, signal strength, register values map, and serializes to JSON |
| `LoadController` | Reads `nextLoadOn` / `nextLoadOff` UTC timestamps from the API response and persists them to NVS. On each cycle, compares current time against the window and toggles the MPPT load output accordingly |
| `TimeService` | Syncs ESP32 system clock from the API response (`currentTime` field). Restores approximate time after deep sleep using `storedEpoch + DEEP_SLEEP_DURATION`. Provides `parseISO8601` and interval tracking for modem usage |
| `SleepManager` | Saves total awake time to NVS before sleep, restores it after wake. Stores epoch to RTC memory so `TimeService` can restore the clock without a modem sync |

---

## RS485 / Modbus Register Map

The MPPT is addressed at Modbus slave ID **1**. All reads use **Function 0x04** (Read Input Registers) except where noted.

### Input Registers (0x04)

| Address | Name | Scale | Type | Unit |
|---|---|---|---|---|
| 0x3100 | PV Input Voltage | ×0.01 | U16 | V |
| 0x3101 | PV Input Current | ×0.01 | U16 | A |
| 0x3102 | PV Input Power | ×0.01 | U32 | W |
| 0x3108 | Battery Voltage | ×0.01 | U16 | V |
| 0x3109 | Battery Output Current | ×0.01 | U16 | A |
| 0x310A | Battery Output Power | ×0.01 | U32 | W |
| 0x310C | Load Output Voltage | ×0.01 | U16 | V |
| 0x310D | Load Output Current | ×0.01 | U16 | A |
| 0x310E | Load Output Power | ×0.01 | U32 | W |
| 0x3110 | Remote Battery Temperature | ×0.01 | S16 | °C |
| 0x3111 | Equipment Temperature | ×0.01 | S16 | °C |
| 0x3112 | MOSFET Temperature | ×0.01 | S16 | °C |
| 0x311A | Battery SOC | ×1.0 | U16 | % |
| 0x3200 | Battery Status (flags) | ×1.0 | U16 | — |
| 0x3201 | Equipment Charging Status | ×1.0 | U16 | — |
| 0x3202 | Equipment Discharging Status | ×1.0 | U16 | — |
| 0x3300 | Max PV Voltage Today | ×0.01 | U16 | V |
| 0x3301 | Min PV Voltage Today | ×0.01 | U16 | V |
| 0x3302 | Max Battery Voltage Today | ×0.01 | U16 | V |
| 0x3303 | Min Battery Voltage Today | ×0.01 | U16 | V |
| 0x3304 | Consumed Energy Today | ×0.01 | U32 | kWh |
| 0x3306 | Consumed Energy This Month | ×0.01 | U32 | kWh |
| 0x3308 | Consumed Energy This Year | ×0.01 | U32 | kWh |
| 0x330A | Total Consumed Energy | ×0.01 | U32 | kWh |
| 0x330C | Generated Energy Today | ×0.01 | U32 | kWh |
| 0x330E | Generated Energy This Month | ×0.01 | U32 | kWh |
| 0x3310 | Generated Energy This Year | ×0.01 | U32 | kWh |
| 0x3312 | Total Generated Energy | ×0.01 | U32 | kWh |
| 0x331B | Battery Current | ×0.01 | S32 | A |

### Holding Registers (0x03 / 0x06)

| Address | Name | Notes |
|---|---|---|
| 0x903D | Load Control Mode | 0=Manual, 1=Sunset ON, 2=Sunset+Timer, 3=Timer, 6=Always ON |
| 0x9013 | RTC Second / Minute | D[7:0]=Second, D[15:8]=Minute |
| 0x9014 | RTC Hour / Day | D[7:0]=Hour, D[15:8]=Day |
| 0x9015 | RTC Month / Year | D[7:0]=Month, D[15:8]=Year (offset from 2000) |

### Coils (0x01 / 0x05)

| Address | Name | Notes |
|---|---|---|
| 0x0002 | Load Output Control | 0=OFF, 1=ON (remote control) |

---

## Load Control Logic

The backend API (`mppt-be-controller`) calculates optimal on/off windows based on the solar energy generated that day and the expected night duration. It returns:

```json
{
  "nextLoadOn":  "2025-07-23T21:15:00+00:00",
  "nextLoadOff": "2025-07-24T04:30:00+00:00",
  "currentTime": "2025-07-23T19:01:42+00:00"
}
```

`LoadController` stores `nextLoadOn` and `nextLoadOff` as UTC epoch values in NVS so they survive deep sleep. On every wake cycle it:

1. Reads the current load state from the MPPT coil.
2. Reads battery SOC and temperature.
3. Checks whether `currentTime` falls inside the `[nextLoadOn, nextLoadOff)` window (handles overnight windows where `nextLoadOn > nextLoadOff`).
4. Writes to the MPPT load coil only if the state needs to change.

If reading the load state fails, the load is turned **off** as a safe default.

---

## OTA Firmware Updates

OTA is performed in-place using the ESP32 `Update` library over a standard HTTP connection (no TLS required for firmware chunks).

**Flow:**

1. On each modem-active cycle, `performOtaUpdate()` fetches `GET /firmware.json`.
2. The response contains a `version` string, `total_size`, and a list of `parts` (chunked URLs + sizes).
3. If `version` matches the compiled `MPPT_FIRMWARE_VERSION`, OTA is skipped.
4. Otherwise `Update.begin(total_size)` is called and each chunk is downloaded sequentially via HTTPS and written with `Update.write()`.
5. On success, `Update.end()` is called and the ESP32 restarts.

Firmware files on the server must follow the naming convention: `firmware_<version>.bin` (e.g. `firmware_1.1.6.bin`).

To deploy a new version:
1. Bump `MPPT_FIRMWARE_VERSION` in `platformio.ini`.
2. Build — `rename_firmware.py` automatically copies the output to `build_output/firmware_<version>.bin`.
3. Upload `firmware_<version>.bin` to the `firmware/` directory on the server.
4. The device will pick it up on its next modem-active wake cycle.

---

## Configuration

### secrets.h

Copy `include/secrets.h.template` to `include/secrets.h` (it is gitignored) and fill in:

```cpp
#define TELEGRAM_HTTP_USER  "your_telegraf_user"
#define TELEGRAM_HTTP_PASS  "your_telegraf_password"
```

### Build Flags

All environment-specific values are set as build flags in `platformio.ini`:

| Flag | Example | Description |
|---|---|---|
| `MPPT_FIRMWARE_VERSION` | `"1.1.6"` | Embedded version string, checked during OTA |
| `TINY_GSM_MODEM_A7670` | *(defined)* | Selects the A7670E modem implementation |

### Key Constants (Globals.h)

| Constant | Default | Description |
|---|---|---|
| `DEEP_SLEEP_DURATION` | `120` s | Time between wake cycles |
| `SEND_INTERVAL_SEC` | `900` s (15 min) | Minimum interval between modem activations |
| `HTTP_TELEGRAF_SERVER` | `telegraf-mppt.igerko.com` | Telegraf ingest endpoint host |
| `HTTP_TELEGRAF_PORT` | `80` | Telegraf HTTP port |
| `HTTP_MPPT_SERVER` | `mppt.igerko.com` | Backend API host |
| `HTTP_MPPT_PORT` | `80` | Backend API port |
| `OTA_SERVER` | `mppt.igerko.com` | OTA firmware host |
| `MPPT_LOG_FILE_NAME` | `/mppt_log.log` | LittleFS log file path |
| `MY_ESP_DEVICE_ID` | `"crss"` | Device identifier sent in every payload |
| `PREF_NAME` | `"crss-pref"` | NVS namespace |
| `NETWORK_APN` | `"internet"` | SIM card APN |

---

## Pin Assignment

Applies to the **LILYGO TTGO T-A7670X R2** board with the A7670E modem:

| Signal | GPIO |
|---|---|
| RS485 RX | 19 |
| RS485 TX | 18 |
| RS485 DE/RE | 23 |
| Modem TX | 26 |
| Modem RX | 27 |
| Modem DTR | 25 |
| Modem PWRKEY | 4 |
| Modem POWERON | 12 |
| Modem RESET | 5 |
| Modem RING | 33 |
| Battery ADC | 35 |
| Solar ADC | 36 |

---

## Building & Flashing

**Requirements:**
- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- `include/secrets.h` present (copy from template)

```bash
# Build firmware
pio run -e T-A7670X

# Upload firmware + filesystem
pio run -e T-A7670X --target upload
pio run -e T-A7670X --target uploadfs

# Monitor serial output
pio device monitor
```

The post-build script `rename_firmware.py` automatically copies the compiled binary to:
```
build_output/firmware_<MPPT_FIRMWARE_VERSION>.bin
```

---

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| [ModbusMaster](https://github.com/4-20ma/ModbusMaster) | ^2.0.1 | RS485 Modbus RTU master |
| [ArduinoJson](https://arduinojson.org/) | ^7.4.2 | JSON serialization / deserialization |
| [ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient) | ^0.6.1 | HTTP client over TinyGSM transport |
| [StreamDebugger](https://github.com/vshymanskyy/StreamDebugger) | ^1.0.1 | AT command debugging (optional) |
| TinyGSM | *(bundled with espressif32 platform)* | Modem abstraction layer |

Platform: `espressif32 @ 6.11.0`, Framework: Arduino, Standard: GNU++17

---

## File System

LittleFS is used for log persistence across deep sleep cycles.

```
/
└── mppt_log.log      ← newline-delimited JSON, one LogEntry per line
```

Each line is a JSON object:

```json
{
  "ts": 1753296102,
  "device_id": "crss",
  "signal": 74,
  "total_wake_time": 3821,
  "load_status": 1,
  "modem_sync_time": 1753295700,
  "firmware_version": "1.1.6",
  "registers": {
    "0x3100": 18.42,
    "0x3108": 12.61,
    "0x311A": 87.0,
    ...
  }
}
```

Lines that fail to send (non-2xx response) are written to a `.tmp` file and promoted back to the main log on the next cycle.

---

## Deep Sleep & Time Keeping

Time is managed without an external RTC chip:

1. **First boot / clock loss:** `TimeService::isTimeToUseModem()` detects an invalid epoch (< 2020-01-01) and forces modem activation. The API response `currentTime` field sets the ESP32 system clock via `settimeofday()`.

2. **After deep sleep:** `storedEpoch` is written to `RTC_DATA_ATTR` memory before sleep. On wake, `TimeService::setTimeAfterWakeUp()` restores the clock as `storedEpoch + DEEP_SLEEP_DURATION` — accurate enough for the 15-minute send interval check without needing a modem connection every cycle.

3. **MPPT RTC sync:** After each modem-assisted time sync, the current local time (CET/CEST) is written to the MPPT controller's holding registers so the MPPT's internal daily stats reset at the correct local midnight.

Timezone is configured at startup:
```cpp
setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
tzset();
```
