#pragma once

#include "LoggingService.h"

enum RegType { REG_U16, REG_U32, REG_S16, REG_S32 };

struct RegisterInfo {
  uint16_t    address;
  const char* name;
  float       scale;
  RegType     type;
};

struct HoldingRegisterInfo {
  uint16_t    address;
  const char* name;
};

inline RegisterInfo regBatterySoc  = {0x311A, "Battery SOC (%)", 1.0f, REG_U16};
inline RegisterInfo regBatteryTemp = {0x3110, "Battery Temp (°C)", 0.01f, REG_U16};

constexpr RegisterInfo mpptReadRegisters[] = {
    // 🔋 Battery status
    {0x3108, "Battery Voltage (V)", 0.01f, REG_U16},
    {0x3109, "Battery Output Current (A)", 0.01f, REG_U16},
    {0x310A, "Battery Output Power (W)", 0.01f, REG_U32},
    {0x311A, "Battery SOC (%)", 1.0f, REG_U16},
    {0x331B, "Battery Current (A)", 0.01f, REG_S32},

    // ⚡ Load
    {0x310C, "Load Output Voltage (V)", 0.01f, REG_U16},
    {0x310D, "Load Output Current (A)", 0.01f, REG_U16},
    {0x310E, "Load Output Power (W)", 0.01f, REG_U32},

    // ☀️ PV input
    {0x3100, "PV Input Voltage (V)", 0.01f, REG_U16},
    {0x3101, "PV Input Current (A)", 0.01f, REG_U16},
    {0x3102, "PV Input Power (W)", 0.01f, REG_U32},

    // 🌡️ Temps
    {0x3110, "Remote Battery Temperature (°C)", 0.01f, REG_S16},
    {0x3111, "Equipment Temperature (°C)", 0.01f, REG_S16},
    {0x3112, "MOSFET Temperature (°C)", 0.01f, REG_S16},

    // 📊 PV & Battery voltage min/max
    {0x3300, "Max PV Volt Today (V)", 0.01f, REG_U16},
    {0x3301, "Min PV Volt Today (V)", 0.01f, REG_U16},
    {0x3302, "Max Battery Volt Today (V)", 0.01f, REG_U16},
    {0x3303, "Min Battery Volt Today (V)", 0.01f, REG_U16},

    // 📈 Consume stats
    {0x3304, "Consumed Energy Today (kWh)", 0.01f, REG_U32},
    {0x3306, "Consumed Energy This Month (kWh)", 0.01f, REG_U32},
    {0x3308, "Consumed Energy This Year (kWh)", 0.01f, REG_U32},
    {0x330A, "Total Consumed Energy (kWh)", 0.01f, REG_U32},

    // 📈 Generate stats
    {0x330C, "Generated Energy Today (kWh)", 0.01f, REG_U32},
    {0x330E, "Generated Energy This Month (kWh)", 0.01f, REG_U32},
    {0x3310, "Generated Energy This Year (kWh)", 0.01f, REG_U32},
    {0x3312, "Total Generated Energy (kWh)", 0.01f, REG_U32},

    // ⚠️ State registers (flag values)
    {0x3200, "Battery Status (flags)", 1.0f, REG_U16},
    {0x3201, "Equipment Charging Status (flags)", 1.0f, REG_U16},
    {0x3202, "Equipment Discharging Status (flags)", 1.0f, REG_U16},
};

struct DateTimeFields {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint8_t year;  // maybe offset, depends on device spec
};

constexpr uint16_t            HR_RTC_SecondMinute    = 0x9013;  // D7–0: Second, D15–8: Minute
constexpr uint16_t            HR_RTC_HourDay         = 0x9014;  // D7–0: Hour,  D15–8: Day
constexpr uint16_t            HR_RTC_MonthYear       = 0x9015;  // D7–0: Month, D15–8: Year
constexpr uint16_t            HR_LoadControlMode     = 0x903D;
constexpr HoldingRegisterInfo mpptHoldingRegisters[] = {
    {HR_LoadControlMode, "Load Control Mode"},
    /*
      0: Manual Mode (Default)
      1: Sunset Load ON Mode
      2: Sunset Load ON + Timer Mode
      3: Timer Mode
      6: Always ON Mode
    */
};

constexpr int mpptRegistersCount = std::size(mpptReadRegisters) + std::size(mpptHoldingRegisters);

class SolarMPPTMonitor {
 public:
  SolarMPPTMonitor();
  static void setupRS465();

  static LogEntry readLogsFromMPPT();
  static bool     setDatetimeInMPPT();
  static bool     readLoadState(int& loadState);
  static bool     setLoad(bool enable);
  static bool     readBatteryStatus(float& socPercent, float& tempC);

 private:
  static bool readRegister(const RegisterInfo& reg, float& outValue);
  static bool readHoldingRegister(uint16_t address, uint16_t& outValue);
  static bool writeHoldingRegister(uint16_t address, uint16_t value);

  static bool readDatetimeInMPPT(DateTimeFields& dt);
};
