#include "WeatherStation.h"
#include <string.h>
#include <stdlib.h>

static bool parseHexStringLocal(const String& input, uint8_t* out, size_t& outLen, size_t maxLen) {
  String s = input;
  s.trim();
  s.replace(" ", "");
  s.replace("\t", "");
  s.replace("\r", "");
  s.replace("\n", "");

  if (s.length() == 0 || (s.length() % 2) != 0) {
    outLen = 0;
    return false;
  }

  const size_t bytes = s.length() / 2;
  if (bytes > maxLen) {
    outLen = 0;
    return false;
  }

  outLen = 0;
  for (size_t i = 0; i < bytes; i++) {
    const String tok = s.substring(i * 2, i * 2 + 2);
    char* endptr = nullptr;
    const unsigned long v = strtoul(tok.c_str(), &endptr, 16);
    if (*endptr != '\0' || v > 0xFFUL) {
      outLen = 0;
      return false;
    }
    out[outLen++] = static_cast<uint8_t>(v);
  }

  return true;
}

WeatherStation::WeatherStation(HardwareSerial& serial)
    : _serial(serial) {}

void WeatherStation::begin() {
  _serial.begin(_config.baudrate, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
}

bool WeatherStation::setBaudrate(uint32_t baud) {
  _config.baudrate = baud;
  _serial.updateBaudRate(baud);
  return true;
}

uint16_t WeatherStation::crc16(const uint8_t* data, size_t len) const {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; pos++) {
    crc ^= static_cast<uint16_t>(data[pos]);
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

int16_t WeatherStation::toSigned(uint16_t v) {
  return static_cast<int16_t>(v);
}

String WeatherStation::toHex(const uint8_t* data, size_t len) {
  static const char* HEX_CHARS = "0123456789ABCDEF";
  String out;
  out.reserve(len * 3);
  for (size_t i = 0; i < len; i++) {
    if (i) out += ' ';
    out += HEX_CHARS[(data[i] >> 4) & 0x0F];
    out += HEX_CHARS[data[i] & 0x0F];
  }
  return out;
}

const __FlashStringHelper* WeatherStation::exceptionText(uint8_t code) {
  switch (code) {
    case 0x01: return F("illegal function");
    case 0x02: return F("illegal data address");
    case 0x03: return F("illegal data value");
    case 0x04: return F("slave device failure");
    case 0x05: return F("acknowledge");
    case 0x06: return F("slave device busy");
    case 0x08: return F("memory parity error");
    case 0x0A: return F("gateway path unavailable");
    case 0x0B: return F("gateway target failed");
    default:   return F("modbus exception");
  }
}

void WeatherStation::setSimpleError(const String& error) {
  _commLog.lastOk = false;
  _commLog.lastDurationMs = 0;
  _commLog.lastAction = String();
  _commLog.lastRequestHex = String();
  _commLog.lastResponseHex = String();
  _commLog.lastError = error;
  _commLog.errorCount++;
}

void WeatherStation::setCommResult(
    bool ok,
    uint32_t durationMs,
    const String& action,
    const uint8_t* req,
    size_t reqLen,
    const uint8_t* resp,
    size_t respLen,
    const String& error) {
  _commLog.lastOk = ok;
  _commLog.lastDurationMs = durationMs;
  _commLog.lastAction = action;
  _commLog.lastRequestHex = req ? toHex(req, reqLen) : String();
  _commLog.lastResponseHex = resp ? toHex(resp, respLen) : String();
  _commLog.lastError = error;
  if (ok) {
    _commLog.successCount++;
  } else {
    _commLog.errorCount++;
  }
}

bool WeatherStation::transact(
    const uint8_t* request,
    size_t reqLen,
    uint8_t* response,
    size_t responseCap,
    uint32_t timeoutMs,
    const String& action,
    size_t* actualLen) {
  while (_serial.available()) {
    _serial.read();
  }

  const uint32_t t0 = millis();
  _serial.write(request, reqLen);
  _serial.flush();

  size_t got = 0;
  uint32_t lastByteAt = 0;
  bool seenAnyByte = false;

  while ((millis() - t0) < timeoutMs && got < responseCap) {
    bool gotByteNow = false;

    while (_serial.available() && got < responseCap) {
      response[got++] = static_cast<uint8_t>(_serial.read());
      lastByteAt = millis();
      seenAnyByte = true;
      gotByteNow = true;
    }

    if (seenAnyByte && !gotByteNow && (millis() - lastByteAt) >= 20) {
      break;
    }
    delay(1);
  }

  if (actualLen) {
    *actualLen = got;
  }

  if (!seenAnyByte) {
    setCommResult(false, millis() - t0, action, request, reqLen, response, 0, F("timeout"));
    return false;
  }

  setCommResult(true, millis() - t0, action, request, reqLen, response, got, String());
  return true;
}

bool WeatherStation::sendRawFrame(const uint8_t* request, size_t reqLen, uint8_t* response, size_t responseCapacity, uint32_t timeoutMs, const String& action) {
  if (request == nullptr || reqLen == 0) {
    setSimpleError(F("invalid args"));
    return false;
  }

  uint8_t frame[260] = {0};
  if (reqLen > sizeof(frame)) {
    setSimpleError(F("raw frame too long"));
    return false;
  }

  memcpy(frame, request, reqLen);
  size_t frameLen = reqLen;

  bool hasValidCrc = false;
  if (frameLen >= 4) {
    const uint16_t crcProvided = static_cast<uint16_t>(frame[frameLen - 2]) | (static_cast<uint16_t>(frame[frameLen - 1]) << 8);
    const uint16_t crcCalculated = crc16(frame, frameLen - 2);
    hasValidCrc = (crcProvided == crcCalculated);
  }

  if (!hasValidCrc) {
    if ((frameLen + 2) > sizeof(frame)) {
      setSimpleError(F("raw frame too long"));
      return false;
    }
    const uint16_t crc = crc16(frame, frameLen);
    frame[frameLen++] = static_cast<uint8_t>(crc & 0xFF);
    frame[frameLen++] = static_cast<uint8_t>((crc >> 8) & 0xFF);
  }

  size_t actualLen = 0;
  const uint32_t effectiveTimeoutMs = timeoutMs < 2000 ? 2000 : timeoutMs;
  return transact(frame, frameLen, response, responseCapacity, effectiveTimeoutMs, action, &actualLen);
}

bool WeatherStation::sendRawFrame(const uint8_t* request, size_t reqLen, uint32_t timeoutMs, const String& action) {
  uint8_t resp[128] = {0};
  return sendRawFrame(request, reqLen, resp, sizeof(resp), timeoutMs, action);
}

bool WeatherStation::readHoldingRegisters(uint16_t startReg, uint16_t count, uint16_t* out) {
  if (count == 0 || out == nullptr) {
    setSimpleError(F("invalid args"));
    return false;
  }

  uint8_t req[8];
  req[0] = _config.address;
  req[1] = 0x03;
  req[2] = static_cast<uint8_t>((startReg >> 8) & 0xFF);
  req[3] = static_cast<uint8_t>(startReg & 0xFF);
  req[4] = static_cast<uint8_t>((count >> 8) & 0xFF);
  req[5] = static_cast<uint8_t>(count & 0xFF);
  const uint16_t crc = crc16(req, 6);
  req[6] = static_cast<uint8_t>(crc & 0xFF);
  req[7] = static_cast<uint8_t>((crc >> 8) & 0xFF);

  uint8_t resp[64] = {0};
  size_t respLen = 0;

  char regHex[5];
  snprintf(regHex, sizeof(regHex), "%04X", startReg);
  String action = F("read 0x");
  action += regHex;
  action += F(" + ");
  action += String(count);

  if (!transact(req, sizeof(req), resp, sizeof(resp), 2000, action, &respLen)) {
    return false;
  }

  if (respLen < 5) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("short response"));
    return false;
  }

  if (resp[0] != _config.address) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("wrong slave address"));
    return false;
  }

  const uint16_t crcResp = static_cast<uint16_t>(resp[respLen - 2]) | (static_cast<uint16_t>(resp[respLen - 1]) << 8);
  const uint16_t crcCalc = crc16(resp, respLen - 2);
  if (crcResp != crcCalc) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("crc mismatch"));
    return false;
  }

  if (resp[1] == static_cast<uint8_t>(0x03 | 0x80)) {
    String err = F("modbus exception: ");
    err += String(exceptionText(resp[2]));
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, err);
    return false;
  }

  if (resp[1] != 0x03) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("bad function"));
    return false;
  }

  const size_t expectedLen = 5 + static_cast<size_t>(count) * 2;
  if (respLen != expectedLen) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("bad length"));
    return false;
  }

  if (resp[2] != count * 2) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("bad byte count"));
    return false;
  }

  for (uint16_t i = 0; i < count; i++) {
    out[i] = (static_cast<uint16_t>(resp[3 + i * 2]) << 8) | static_cast<uint16_t>(resp[4 + i * 2]);
  }

  _commLog.lastOk = true;
  _commLog.lastError = String();
  return true;
}

bool WeatherStation::writeRegister(uint16_t reg, uint16_t value) {
  uint8_t req[8];
  req[0] = _config.address;
  req[1] = 0x06;
  req[2] = static_cast<uint8_t>((reg >> 8) & 0xFF);
  req[3] = static_cast<uint8_t>(reg & 0xFF);
  req[4] = static_cast<uint8_t>((value >> 8) & 0xFF);
  req[5] = static_cast<uint8_t>(value & 0xFF);
  const uint16_t crc = crc16(req, 6);
  req[6] = static_cast<uint8_t>(crc & 0xFF);
  req[7] = static_cast<uint8_t>((crc >> 8) & 0xFF);

  uint8_t resp[16] = {0};
  size_t respLen = 0;

  char regHex[5];
  char valHex[5];
  snprintf(regHex, sizeof(regHex), "%04X", reg);
  snprintf(valHex, sizeof(valHex), "%04X", value);
  String action = F("write 0x");
  action += regHex;
  action += F(" = 0x");
  action += valHex;

  if (!transact(req, sizeof(req), resp, sizeof(resp), 2000, action, &respLen)) {
    return false;
  }

  if (respLen < 5) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("short response"));
    return false;
  }

  if (resp[0] != _config.address) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("wrong slave address"));
    return false;
  }

  const uint16_t crcResp = static_cast<uint16_t>(resp[respLen - 2]) | (static_cast<uint16_t>(resp[respLen - 1]) << 8);
  const uint16_t crcCalc = crc16(resp, respLen - 2);
  if (crcResp != crcCalc) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("crc mismatch"));
    return false;
  }

  if (resp[1] == static_cast<uint8_t>(0x06 | 0x80)) {
    String err = F("modbus exception: ");
    err += String(exceptionText(resp[2]));
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, err);
    return false;
  }

  if (resp[1] != 0x06) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("bad function"));
    return false;
  }

  if (respLen != 8) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("bad length"));
    return false;
  }

  if (memcmp(req, resp, 6) != 0) {
    setCommResult(false, _commLog.lastDurationMs, action, req, sizeof(req), resp, respLen, F("write echo mismatch"));
    return false;
  }

  _commLog.lastOk = true;
  _commLog.lastError = String();
  return true;
}

bool WeatherStation::readAll() {
  uint16_t regs[REG_DATA_COUNT] = {0};
  if (!readHoldingRegisters(REG_DATA_START, REG_DATA_COUNT, regs)) {
    _values.registersValid = false;
    return false;
  }

  for (uint8_t i = 0; i < REG_DATA_COUNT; i++) {
    _values.raw[i] = regs[i];
  }

  _values.registersValid = true;
  _values.lastReadMs = millis();
  decodeRegisters();
  detectPresence();
  return true;
}

bool WeatherStation::readSensorConfig() {
  uint16_t regs[4] = {0};
  if (!readHoldingRegisters(REG_CFG_WIND_DIR_OFFSET, 4, regs)) {
    _config.sensorConfigReadOk = false;
    return false;
  }

  _config.windOffset = regs[0];
  _config.rainSensitivity = regs[3];
  _config.sensorConfigReadOk = true;
  return true;
}

void WeatherStation::decodeRegisters() {
  _values.windSpeed = _values.raw[0] / 10.0f;
  _values.windForce = static_cast<int>(_values.raw[1]);
  _values.windDir8 = static_cast<int>(_values.raw[2]);
  _values.windDirDeg = static_cast<int>(_values.raw[3]);
  _values.humidity = _values.raw[4] / 10.0f;
  _values.temperature = toSigned(_values.raw[5]) / 10.0f;
  _values.noise = _values.raw[6] / 10.0f;
  _values.air1 = static_cast<int>(_values.raw[7]);
  _values.air2 = static_cast<int>(_values.raw[8]);
  _values.pressureKpa = _values.raw[9] / 10.0f;
  _values.lightHigh = _values.raw[10];
  _values.lightLow = _values.raw[11];
  _values.lightLux = _values.raw[12];
  _values.rain = _values.raw[13] / 10.0f;
  _values.compassDeg = _values.raw[14] / 100.0f;
  _values.solarRadiation = static_cast<int>(_values.raw[15]);
}

void WeatherStation::detectPresence() {
  _presence.wind =
      (_values.windSpeed >= 0.0f) &&
      (_values.windDirDeg >= 0) &&
      (_values.windDirDeg <= 359);

  _presence.humidityTemp =
      isfinite(_values.humidity) &&
      isfinite(_values.temperature) &&
      (_values.humidity >= 0.0f && _values.humidity <= 100.0f) &&
      (_values.temperature > -50.0f && _values.temperature < 90.0f);

  if (isfinite(_values.noise) &&
      _values.noise >= 29.5f &&
      _values.noise <= 30.5f) {
    _presence.noise = false;
    _values.noise = NAN;
  } else {
    _presence.noise = isfinite(_values.noise) && (_values.noise > 0.0f) && (_values.noise <= 150.0f);
  }

  _presence.air = (_values.air1 > 0 || _values.air2 > 0);
  if (!_presence.air) {
    _values.air1 = -1;
    _values.air2 = -1;
  }

  _presence.pressure = isfinite(_values.pressureKpa) && (_values.pressureKpa > 10.0f);
  if (!_presence.pressure) _values.pressureKpa = NAN;

  _presence.light = (_values.lightLux > 0) || (_values.lightHigh > 0) || (_values.lightLow > 0);
  if (!_presence.light) {
    _values.lightLux = 0;
    _values.lightHigh = 0;
    _values.lightLow = 0;
  }

  _presence.rain = isfinite(_values.rain) && (_values.rain >= 0.0f);
  if (!_presence.rain) _values.rain = NAN;

  _presence.compass = isfinite(_values.compassDeg) && (_values.compassDeg > 0.01f);
  if (!_presence.compass) _values.compassDeg = NAN;

  _presence.solar = (_values.solarRadiation > 0);
  if (!_presence.solar) _values.solarRadiation = -1;
}

bool WeatherStation::commandZeroWind() {
  uint8_t req[64];
  size_t reqLen = 0;

  if (!parseHexStringLocal(String(CMD_WIND_ZERO), req, reqLen, sizeof(req))) {
    setSimpleError(F("bad wind zero prefix"));
    return false;
  }

  if (reqLen + 2 > sizeof(req)) {
    setSimpleError(F("wind zero frame too long"));
    return false;
  }

  const uint16_t crc = crc16(req, reqLen);
  req[reqLen++] = static_cast<uint8_t>(crc & 0xFF);
  req[reqLen++] = static_cast<uint8_t>((crc >> 8) & 0xFF);

  uint8_t resp[32] = {0};
  size_t respLen = 0;

  if (!transact(req, reqLen, resp, sizeof(resp), 2000, F("wind zero raw"), &respLen)) {
    return false;
  }

  if (respLen == 0) {
    setCommResult(false, _commLog.lastDurationMs, F("wind zero raw"), req, reqLen, resp, respLen, F("no response"));
    return false;
  }

  _commLog.lastOk = true;
  _commLog.lastError = String();
  return true;
}

bool WeatherStation::commandZeroRain() {
  return writeRegister(REG_CFG_RAIN_ZERO, CFG_RAIN_ZERO);
}

void WeatherStation::pushConfigLog() {
  _lastConfigLog = _commLog;
  _hasConfigLog = true;
}

const CommLog& WeatherStation::configLogAt(size_t reverseIndex) const {
  static const CommLog empty;
  if (!_hasConfigLog || reverseIndex != 0) {
    return empty;
  }
  return _lastConfigLog;
}

String WeatherStation::airLabel1() const {
  if (_config.airMode == AirQualityMode::CO2) return F("CO2");
  return F("PM2.5");
}

String WeatherStation::airLabel2() const {
  if (_config.airMode == AirQualityMode::CO2) return F("CO2");
  return F("PM10");
}