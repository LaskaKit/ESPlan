/*
 * mb_client.cpp – Implementace Modbus RTU klienta
 */

#include "mb_client.h"

uint16_t ModbusClient::crc16(const uint8_t* data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
      else              crc >>= 1;
    }
  }
  return crc;
}

void ModbusClient::flushRx() {
  while (_serial->available()) _serial->read();
}

void ModbusClient::begin(uint32_t baud, uint8_t /*addr*/) {
  _baud   = baud;
  _serial = &Serial2;
  _serial->begin(_baud, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  Serial.printf("[MOD] RS485 init: %d baud, 8N1, RX=%d TX=%d\n",
                _baud, PIN_RS485_RX, PIN_RS485_TX);
}

void ModbusClient::setBaudRate(uint32_t baud) {
  _baud = baud;
  _serial->end();
  _serial->begin(_baud, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  Serial.printf("[MOD] Baudrate zmenen na: %d\n", _baud);
}

ModbusStatus ModbusClient::readRegisters(uint8_t slaveAddr, uint16_t startReg,
                                          uint8_t count, uint16_t* out) {
  if (!_serial || count == 0 || count > 64) return ModbusStatus::FRAME_ERROR;

  uint8_t req[8];
  req[0] = slaveAddr;
  req[1] = 0x03;
  req[2] = (startReg >> 8) & 0xFF;
  req[3] = startReg & 0xFF;
  req[4] = 0x00;
  req[5] = count;
  uint16_t c = crc16(req, 6);
  req[6] = c & 0xFF;
  req[7] = (c >> 8) & 0xFF;

  // Ulozit TX
  memcpy(lastTx, req, 8);
  lastTxLen = 8;

  flushRx();
  _serial->write(req, 8);
  _serial->flush();

  uint8_t  expectedLen = 3 + count * 2 + 2;
  uint8_t  buf[160];
  uint8_t  rxLen = 0;
  uint32_t t0    = millis();

  while (rxLen < expectedLen && (millis() - t0 < TIMEOUT_MS)) {
    if (_serial->available()) buf[rxLen++] = _serial->read();
    yield();
  }

  // Ulozit RX
  uint8_t copyLen = (rxLen < sizeof(lastRx)) ? rxLen : sizeof(lastRx);
  memcpy(lastRx, buf, copyLen);
  lastRxLen = copyLen;

  if (rxLen < expectedLen) {
    lastResult = ModbusStatus::TIMEOUT;
    return ModbusStatus::TIMEOUT;
  }

  uint16_t rxCrc   = (uint16_t)buf[rxLen-1] << 8 | buf[rxLen-2];
  uint16_t calcCrc = crc16(buf, rxLen - 2);
  if (rxCrc != calcCrc) {
    lastResult = ModbusStatus::CRC_ERROR;
    return ModbusStatus::CRC_ERROR;
  }

  if (buf[1] & 0x80) {
    lastResult = ModbusStatus::EXCEPTION;
    return ModbusStatus::EXCEPTION;
  }

  lastResult = ModbusStatus::OK;
  for (uint8_t i = 0; i < count; i++) {
    out[i] = (uint16_t)buf[3 + i*2] << 8 | buf[4 + i*2];
  }
  return ModbusStatus::OK;
}

ModbusStatus ModbusClient::writeRegister(uint8_t slaveAddr, uint16_t reg, uint16_t value) {
  if (!_serial) return ModbusStatus::FRAME_ERROR;

  uint8_t req[8];
  req[0] = slaveAddr;
  req[1] = 0x06;
  req[2] = (reg >> 8) & 0xFF;
  req[3] = reg & 0xFF;
  req[4] = (value >> 8) & 0xFF;
  req[5] = value & 0xFF;
  uint16_t c = crc16(req, 6);
  req[6] = c & 0xFF;
  req[7] = (c >> 8) & 0xFF;

  memcpy(lastTx, req, 8);
  lastTxLen = 8;

  flushRx();
  _serial->write(req, 8);
  _serial->flush();

  uint8_t  buf[8];
  uint8_t  rxLen = 0;
  uint32_t t0    = millis();

  while (rxLen < 8 && (millis() - t0 < TIMEOUT_MS)) {
    if (_serial->available()) buf[rxLen++] = _serial->read();
    yield();
  }

  memcpy(lastRx, buf, rxLen);
  lastRxLen = rxLen;

  if (rxLen < 8) { lastResult = ModbusStatus::TIMEOUT; return ModbusStatus::TIMEOUT; }

  uint16_t rxCrc   = (uint16_t)buf[7] << 8 | buf[6];
  uint16_t calcCrc = crc16(buf, 6);
  if (rxCrc != calcCrc) { lastResult = ModbusStatus::CRC_ERROR; return ModbusStatus::CRC_ERROR; }

  lastResult = ModbusStatus::OK;
  return ModbusStatus::OK;
}

const char* ModbusClient::statusStr(ModbusStatus s) {
  switch (s) {
    case ModbusStatus::OK:          return "OK";
    case ModbusStatus::TIMEOUT:     return "TIMEOUT";
    case ModbusStatus::CRC_ERROR:   return "CRC_ERROR";
    case ModbusStatus::EXCEPTION:   return "EXCEPTION";
    case ModbusStatus::FRAME_ERROR: return "FRAME_ERROR";
    default:                        return "UNKNOWN";
  }
}
