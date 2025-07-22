// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <HardwareSerial.h>
#include <ModbusMaster.h>

extern HardwareSerial RS485Serial; // just declare, not define
extern ModbusMaster node;

#endif