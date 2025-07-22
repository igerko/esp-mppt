#include <ModbusMaster.h>
#include <HardwareSerial.h>

HardwareSerial RS485Serial(2);
ModbusMaster node;

#define RS485_RXD   25
#define RS485_TXD   26
#define RS485_DERE  27
#define RS485_BAUD  115200

// prepínanie smeru
void preTransmission() { digitalWrite(RS485_DERE, HIGH); }
void postTransmission() { digitalWrite(RS485_DERE, LOW); }

// štruktúra pre register
enum RegType {
  REG_U16,
  REG_U32
};

struct RegisterInfo {
  uint16_t address; 
  const char* name;
  float scale;
  RegType type;
};

struct HoldingRegisterInfo {
  uint16_t address;
  const char* name;
};

RegisterInfo registers[] = {
  {0x3108, "Battery Voltage (V)",             0.01f, REG_U16},
  {0x3109, "Battery Output Current (A)",      0.01f, REG_U16},
  {0x310A, "Battery Output Power (W)",        0.01f, REG_U32},
  {0x310C, "Load Output Voltage (V)",         0.01f, REG_U16},
  {0x310D, "Load Output Current (A)",         0.01f, REG_U16},
  {0x310E, "Load Output Power (W)",           0.01f, REG_U32},
  {0x3110, "Remote Battery Temperature (°C)", 0.01f, REG_U16},
  {0x3111, "Equipment Temperature (°C)",      0.01f, REG_U16},
  {0x3112, "MOSFET Temperature (°C)",         0.01f,  REG_U16},
  {0x311A, "Battery SOC (%)",                 1.0f,  REG_U16}
};

constexpr uint16_t HR_LoadControlMode = 0x903D;
HoldingRegisterInfo holdingRegisters[] = {
  {HR_LoadControlMode, "Load Control Mode"}, 
  /*
    0: Manual Mode (Default)
    1: Sunset Load ON Mode
    2: Sunset Load ON + Timer Mode
    3: Timer Mode
    6: Always ON Mode
  */
};

void setup() {
  pinMode(RS485_DERE, OUTPUT);
  digitalWrite(RS485_DERE, LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println("RS485 Modbus start...");

  RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD, RS485_TXD);
  node.begin(1, RS485Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

bool readRegister(const RegisterInfo& reg, float &outValue) {
  uint8_t count = (reg.type == REG_U32) ? 2 : 1;

  uint8_t result = node.readInputRegisters(reg.address, count);
  if (result == node.ku8MBSuccess) {
    if (reg.type == REG_U16) {
      uint16_t raw = node.getResponseBuffer(0);
      outValue = raw * reg.scale;
    } else { // REG_U32
      uint16_t low  = node.getResponseBuffer(0);
      uint16_t high = node.getResponseBuffer(1);
      uint32_t combined = ((uint32_t)high << 16) | low;
      outValue = combined * reg.scale;
    }
    return true;
  }
  return false;
}

bool readHoldingRegister(uint16_t address, uint16_t &outValue) {
  uint8_t result = node.readHoldingRegisters(address, 1); // funkcia 0x03
  if (result == node.ku8MBSuccess) {
    outValue = node.getResponseBuffer(0);
    return true;
  }
  return false;
}


bool writeHoldingRegister(uint16_t address, uint16_t value) {
  // Použijeme funkciu 0x06 (write single register)
  uint8_t result = node.writeSingleRegister(address, value);
  if (result == node.ku8MBSuccess) {
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

bool readLoadState(bool &isOn) {
  // coil 0x0002 = Remote control of load
  uint8_t result = node.readCoils(0x0002, 1); // čítame 1 coil od adresy 0x0002
  if (result == node.ku8MBSuccess) {
    isOn = node.getResponseBuffer(0); // vráti 0 alebo 1
    Serial.print("Aktuálny stav LOAD: ");
    Serial.println(isOn ? "ON ✅" : "OFF ❌");
    return true; // čítanie prebehlo úspešne
  } else {
    Serial.print("Chyba pri čítaní LOAD, kód: ");
    Serial.println(result);
    return false; // čítanie zlyhalo
  }
}

bool setLoad(bool enable) {
  uint8_t result = node.writeSingleCoil(0x0002, enable); // Coil 2 = Load control
  if (result == node.ku8MBSuccess) {
    Serial.print("LOAD ");
    Serial.println(enable ? "zapnutý ✅" : "vypnutý ❌");
    return true;
  } else {
    Serial.print("Chyba pri ");
    Serial.print(enable ? "zapínaní" : "vypínaní");
    Serial.print(" LOAD, kód: ");
    Serial.println(result);
    return false;
  }
}

void loop() {
  for (auto &r : registers) {
    float value;
    if (readRegister(r, value)) {
      Serial.print(r.name);
      Serial.print(": ");
      Serial.println(value);
    } else {
      Serial.print("Chyba pri čítaní ");
      Serial.println(r.name);
    }
  }
  Serial.println("-------------------------");

  uint16_t mode;
  if (readHoldingRegister(HR_LoadControlMode, mode)) {
    Serial.print("Load Control Mode: ");
    Serial.println(mode);
  } else {
    Serial.println("Nepodarilo sa prečítať režim.");
  }

  // set load control mode to MANUAL
  writeHoldingRegister(HR_LoadControlMode, 0);

  bool loadOn;
  if (readLoadState(loadOn)) {
    if (loadOn) {
      setLoad(false);
    } else {
      setLoad(true);
    }
  }
  Serial.println("--------------------------------------------------");
  delay(10000);
}