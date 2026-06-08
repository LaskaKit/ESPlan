#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"

struct VariantPresence {
  bool wind = false;
  bool humidityTemp = false;
  bool noise = false;
  bool air = false;
  bool pressure = false;
  bool light = false;
  bool rain = false;
  bool compass = false;
  bool solar = false;
};

struct WeatherValues {
  uint16_t raw[REG_DATA_COUNT] = {0};
  bool registersValid = false;
  uint32_t lastReadMs = 0;

  float windSpeed = NAN;
  int windForce = -1;
  int windDir8 = -1;
  int windDirDeg = -1;
  float humidity = NAN;
  float temperature = NAN;
  float noise = NAN;
  int air1 = -1;
  int air2 = -1;
  float pressureKpa = NAN;
  uint16_t lightHigh = 0;
  uint16_t lightLow = 0;
  uint32_t lightLux = 0;
  float rain = NAN;
  float compassDeg = NAN;
  int solarRadiation = -1;
};

struct RuntimeConfig {
  uint8_t address = DEFAULT_MODBUS_ADDR;
  uint32_t baudrate = DEFAULT_MODBUS_BAUD;
  AirQualityMode airMode = AirQualityMode::Auto;
  uint16_t windOffset = 0;
  uint16_t rainSensitivity = 0x0011;
  bool sensorConfigReadOk = false;
};

struct CommLog {
  bool lastOk = false;
  uint32_t lastDurationMs = 0;
  uint32_t successCount = 0;
  uint32_t errorCount = 0;
  String lastAction;
  String lastRequestHex;
  String lastResponseHex;
  String lastError;
};

class WeatherStation {
public:
  explicit WeatherStation(HardwareSerial& serial);

  void begin();
  bool setBaudrate(uint32_t baud);

  bool readAll();
  bool readSensorConfig();
  bool writeRegister(uint16_t reg, uint16_t value);

  bool commandZeroWind();
  bool commandZeroRain();

  bool sendRawFrame(const uint8_t* request, size_t reqLen, uint8_t* response, size_t responseCapacity, uint32_t timeoutMs, const String& action);
  bool sendRawFrame(const uint8_t* request, size_t reqLen, uint32_t timeoutMs, const String& action);

  void pushConfigLog();
  size_t configLogCount() const { return _hasConfigLog ? 1 : 0; }
  const CommLog& configLogAt(size_t reverseIndex) const;

  const WeatherValues& values() const { return _values; }
  const VariantPresence& presence() const { return _presence; }
  RuntimeConfig& config() { return _config; }
  const RuntimeConfig& config() const { return _config; }
  const CommLog& commLog() const { return _commLog; }

  String airLabel1() const;
  String airLabel2() const;

private:
  HardwareSerial& _serial;
  WeatherValues _values;
  VariantPresence _presence;
  RuntimeConfig _config;
  CommLog _commLog;

  CommLog _lastConfigLog;
  bool _hasConfigLog = false;

  uint16_t crc16(const uint8_t* data, size_t len) const;
  bool transact(const uint8_t* request, size_t reqLen, uint8_t* response, size_t responseCap, uint32_t timeoutMs, const String& action, size_t* actualLen = nullptr);
  bool readHoldingRegisters(uint16_t startReg, uint16_t count, uint16_t* out);
  void decodeRegisters();
  void detectPresence();
  void setCommResult(bool ok, uint32_t durationMs, const String& action, const uint8_t* req, size_t reqLen, const uint8_t* resp, size_t respLen, const String& error);
  void setSimpleError(const String& error);
  static int16_t toSigned(uint16_t v);
  static String toHex(const uint8_t* data, size_t len);
  static const __FlashStringHelper* exceptionText(uint8_t code);
};