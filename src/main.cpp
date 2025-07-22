#include <ModbusMaster.h>
#include <HardwareSerial.h>
#include "SolarMPPTMonitor.h"

HardwareSerial RS485Serial(2); // use UART2
ModbusMaster node;             // single Modbus master
SolarMPPTMonitor mpptMonitor;

void setup()
{
  Serial.begin(115200);
  delay(200);
  mpptMonitor.setupRS465();
}

void loop()
{
  mpptMonitor.readAndPrintRegisters();
  delay(10000);
}