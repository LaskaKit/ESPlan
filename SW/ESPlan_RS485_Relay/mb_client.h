#ifndef RL_MB_CLIENT_H
#define RL_MB_CLIENT_H
/*
 * mb_client.h – Modbus RTU klient pres RS485 / HardwareSerial
 *
 * FC03 – Read Holding Registers
 * FC06 – Write Single Register
 *
 * DE pin rizen hardwarove – nevyzaduje softwarove prepinani.
 */

#include <Arduino.h>
#include "cfg.h"

enum class ModbusStatus : uint8_t {
  OK = 0,
  TIMEOUT,
  CRC_ERROR,
  EXCEPTION,
  FRAME_ERROR
};

class ModbusClient {
public:
  ModbusClient() = default;

  void begin(uint32_t baud = 9600, uint8_t addr = 1);

  ModbusStatus readRegisters(uint8_t slaveAddr, uint16_t startReg,
                              uint8_t count, uint16_t* out);

  ModbusStatus writeRegister(uint8_t slaveAddr, uint16_t reg, uint16_t value);

  void setBaudRate(uint32_t baud);

  const char* statusStr(ModbusStatus s);

  // ── Posledni TX/RX frame ────────────────────────────────────────────
  uint8_t      lastTx[16];
  uint8_t      lastTxLen  = 0;
  uint8_t      lastRx[80];
  uint8_t      lastRxLen  = 0;
  ModbusStatus lastResult = ModbusStatus::OK;

private:
  HardwareSerial* _serial = nullptr;
  uint32_t        _baud   = 9600;
  static const uint16_t TIMEOUT_MS = 500;

  uint16_t crc16(const uint8_t* data, uint8_t len);
  void     flushRx();
};

#endif // RL_MB_CLIENT_H
