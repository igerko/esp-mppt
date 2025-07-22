#include "SolarMPPTMonitor.h"
#include "globals.h"

SolarMPPTMonitor::SolarMPPTMonitor()
{
  // You can do init here if needed
}

void SolarMPPTMonitor::setupRS465()
{
  pinMode(RS485_DERE, OUTPUT);
  digitalWrite(RS485_DERE, LOW);
  RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD, RS485_TXD);
  node.begin(1, RS485Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

void SolarMPPTMonitor::preTransmission() { digitalWrite(RS485_DERE, HIGH); }
void SolarMPPTMonitor::postTransmission() { digitalWrite(RS485_DERE, LOW); }

bool SolarMPPTMonitor::readRegister(const RegisterInfo &reg, float &outValue)
{
  uint8_t count = (reg.type == REG_U32) ? 2 : 1;

  uint8_t result = node.readInputRegisters(reg.address, count);
  if (result == node.ku8MBSuccess)
  {
    if (reg.type == REG_U16)
    {
      uint16_t raw = node.getResponseBuffer(0);
      outValue = raw * reg.scale;
    }
    else
    { // REG_U32
      uint16_t low = node.getResponseBuffer(0);
      uint16_t high = node.getResponseBuffer(1);
      uint32_t combined = ((uint32_t)high << 16) | low;
      outValue = combined * reg.scale;
    }
    return true;
  }
  return false;
}

bool SolarMPPTMonitor::readHoldingRegister(uint16_t address, uint16_t &outValue)
{
  uint8_t result = node.readHoldingRegisters(address, 1); // funkcia 0x03
  if (result == node.ku8MBSuccess)
  {
    outValue = node.getResponseBuffer(0);
    return true;
  }
  return false;
}

bool SolarMPPTMonitor::writeHoldingRegister(uint16_t address, uint16_t value)
{
  // Použijeme funkciu 0x06 (write single register)
  uint8_t result = node.writeSingleRegister(address, value);
  if (result == node.ku8MBSuccess)
  {
    Serial.print("Register 0x");
    Serial.print(address, HEX);
    Serial.print(" nastavený na ");
    Serial.println(value);
    return true;
  }
  Serial.print("Chyba pri zápise do 0x");
  Serial.print(address, HEX);
  Serial.print(": ");
  Serial.println(result);
  return false;
}

bool SolarMPPTMonitor::readLoadState(bool &isOn)
{
  // coil 0x0002 = Remote control of load
  uint8_t result = node.readCoils(0x0002, 1);
  if (result == node.ku8MBSuccess)
  {
    isOn = node.getResponseBuffer(0);
    Serial.print("Aktuálny stav LOAD: ");
    Serial.println(isOn ? "ON ✅" : "OFF ❌");
    return true;
  }
  else
  {
    Serial.print("Chyba pri čítaní LOAD, kód: ");
    Serial.println(result);
    return false;
  }
}

bool SolarMPPTMonitor::setLoad(bool enable)
{
  uint8_t result = node.writeSingleCoil(0x0002, enable); // Coil 2 = Load control
  if (result == node.ku8MBSuccess)
  {
    Serial.print("LOAD ");
    Serial.println(enable ? "zapnutý ✅" : "vypnutý ❌");
    return true;
  }
  else
  {
    Serial.print("Chyba pri ");
    Serial.print(enable ? "zapínaní" : "vypínaní");
    Serial.print(" LOAD, kód: ");
    Serial.println(result);
    return false;
  }
}

void SolarMPPTMonitor::readAndPrintRegisters() // used for debug
{
  for (auto &r : registers)
  {
    float value;
    if (readRegister(r, value))
    {
      Serial.print(r.name);
      Serial.print(": ");
      Serial.println(value);
    }
    else
    {
      Serial.print("Chyba pri čítaní ");
      Serial.println(r.name);
    }
  }
  Serial.println("-------------------------");

  uint16_t mode;
  if (readHoldingRegister(HR_LoadControlMode, mode))
  {
    Serial.print("Load Control Mode: ");
    Serial.println(mode);
  }
  else
  {
    Serial.println("Nepodarilo sa prečítať režim.");
  }

  // set load control mode to MANUAL
  writeHoldingRegister(HR_LoadControlMode, 0);

  bool loadOn;
  if (readLoadState(loadOn))
  {
    if (loadOn)
    {
      setLoad(false);
    }
    else
    {
      setLoad(true);
    }
  }
  Serial.println("--------------------------------------------------");
}