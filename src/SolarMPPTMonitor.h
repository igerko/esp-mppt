#include <cstdint>
#include <ModbusMaster.h>

#ifndef SOLAR_MPPT_MONITOR_H
#define SOLAR_MPPT_MONITOR_H

enum RegType
{
  REG_U16,
  REG_U32
};

struct RegisterInfo
{
  uint16_t address;
  const char *name;
  float scale;
  RegType type;
};

struct HoldingRegisterInfo
{
  uint16_t address;
  const char *name;
};

constexpr RegisterInfo registers[] = {
    {0x3108, "Battery Voltage (V)", 0.01f, REG_U16},
    {0x3109, "Battery Output Current (A)", 0.01f, REG_U16},
    {0x310A, "Battery Output Power (W)", 0.01f, REG_U32},
    {0x310C, "Load Output Voltage (V)", 0.01f, REG_U16},
    {0x310D, "Load Output Current (A)", 0.01f, REG_U16},
    {0x310E, "Load Output Power (W)", 0.01f, REG_U32},
    {0x3110, "Remote Battery Temperature (°C)", 0.01f, REG_U16},
    {0x3111, "Equipment Temperature (°C)", 0.01f, REG_U16},
    {0x3112, "MOSFET Temperature (°C)", 0.01f, REG_U16},
    {0x311A, "Battery SOC (%)", 1.0f, REG_U16},

    // 📊 PV & Battery voltage min/max
    {0x3300, "Max PV Volt Today (V)", 0.01f, REG_U16},
    {0x3301, "Min PV Volt Today (V)", 0.01f, REG_U16},
    {0x3302, "Max Battery Volt Today (V)", 0.01f, REG_U16},
    {0x3303, "Min Battery Volt Today (V)", 0.01f, REG_U16},

    // ⚡ Consumed energy
    {0x3304, "Consumed Energy Today (kWh)", 0.01f, REG_U32},
    {0x3306, "Consumed Energy This Month (kWh)", 0.01f, REG_U32},
    {0x3308, "Consumed Energy This Year (kWh)", 0.01f, REG_U32},
    {0x330A, "Total Consumed Energy (kWh)", 0.01f, REG_U32},

    // ⚡ Generated energy
    {0x330C, "Generated Energy Today (kWh)", 0.01f, REG_U32},
    {0x330E, "Generated Energy This Month (kWh)", 0.01f, REG_U32},
    {0x3310, "Generated Energy This Year (kWh)", 0.01f, REG_U32},
    {0x3312, "Total Generated Energy (kWh)", 0.01f, REG_U32},

    // 🔋 Battery Current (signed!)
    {0x331B, "Battery Current (A)", 0.01f, REG_U32}};

constexpr uint16_t HR_LoadControlMode = 0x903D;
constexpr HoldingRegisterInfo holdingRegisters[] = {
    {HR_LoadControlMode, "Load Control Mode"},
    /*
      0: Manual Mode (Default)
      1: Sunset Load ON Mode
      2: Sunset Load ON + Timer Mode
      3: Timer Mode
      6: Always ON Mode
    */
};

#define RS485_RXD 25
#define RS485_TXD 26
#define RS485_DERE 27
#define RS485_BAUD 115200

class SolarMPPTMonitor
{
public:
  SolarMPPTMonitor();
  void setupRS465();
  void readAndPrintRegisters();
  void logCurrentData();
  void sendLoggedDataMQTT();
  void setDateTimeInMPPT(time_t datetime);

private:
  static void preTransmission();
  static void postTransmission();
  bool readRegister(const RegisterInfo &reg, float &outValue);
  bool readHoldingRegister(uint16_t address, uint16_t &outValue);
  bool writeHoldingRegister(uint16_t address, uint16_t value);
  bool readLoadState(bool &isOn);
  bool setLoad(bool enable);

  void clearLoggedData();
};

#endif