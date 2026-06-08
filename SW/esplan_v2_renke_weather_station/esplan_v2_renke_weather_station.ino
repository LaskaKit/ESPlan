#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <Preferences.h>
#include <WebServer.h>
#include <stdlib.h>
#include "Config.h"
#include "Lang.h"
#include "WeatherStation.h"
#include "WebUi.h"
#include "logo_web.h"

Preferences prefs;
WebServer server(80);
HardwareSerial RS485Serial(2);
WeatherStation station(RS485Serial);

bool ethConnected = false;
bool ethGotIp = false;
unsigned long lastPollMs = 0;
unsigned long lastDetectRecheckMs = 0;
bool uiOnConfigPage = false;
String flashMessage;

static void fwLog(const String& msg) {
  Serial.println(String(F("[FW] ")) + msg);
}


static void sendNoCacheHeaders() {
  server.sendHeader(F("Cache-Control"), F("no-store, no-cache, must-revalidate, max-age=0"));
  server.sendHeader(F("Pragma"), F("no-cache"));
  server.sendHeader(F("Expires"), F("0"));
}

static void loadPreferences() {
  prefs.begin("meteo", true);
  station.config().address = prefs.getUChar("addr", DEFAULT_MODBUS_ADDR);
  station.config().baudrate = prefs.getUInt("baud", DEFAULT_MODBUS_BAUD);
  station.config().airMode = static_cast<AirQualityMode>(prefs.getUChar("airMode", 0));
  prefs.end();
}

static void savePreferences() {
  prefs.begin("meteo", false);
  prefs.putUChar("addr", station.config().address);
  prefs.putUInt("baud", station.config().baudrate);
  prefs.putUChar("airMode", static_cast<uint8_t>(station.config().airMode));
  prefs.end();
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esplan-weather");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      ethConnected = true;
      fwLog(F("ETH link connected"));
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethConnected = true;
      ethGotIp = true;
      fwLog(String(F("ETH IP acquired: ")) + ETH.localIP().toString());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ethConnected = false;
      ethGotIp = false;
      fwLog(F("ETH link disconnected"));
      break;
    case ARDUINO_EVENT_ETH_STOP:
      ethConnected = false;
      ethGotIp = false;
      fwLog(F("ETH stopped"));
      break;
    default:
      break;
  }
}

static UiLang currentLang() {
  if (server.hasArg("lang")) return parseLang(server.arg("lang"));
  return UiLang::CZ;
}

static String currentIp() {
  return ethGotIp ? ETH.localIP().toString() : String("0.0.0.0");
}

static String currentMac() {
  return ETH.macAddress();
}

static void redirectWithMessage(const String& url, UiLang lang, const String& msg) {
  flashMessage = msg;
  const String target = url + (url.indexOf('?') >= 0 ? '&' : '?') + String(F("lang=")) + langCode(lang);
  server.sendHeader("Location", target, true);
  server.send(302, "text/plain", "");
}

static String buildSimpleMessage(UiLang lang, const String& title, const String& detail, bool ok) {
  const auto& t = T(lang);
  String msg;
  msg.reserve(320);
  msg += F("<div><strong>");
  msg += htmlEscape(title);
  msg += F(":</strong> ");
  msg += ok ? String(F("<span class='ok'>")) + t.statusOkShort + F("</span>") : String(F("<span class='bad'>")) + t.statusErrorShort + F("</span>");
  if (detail.length()) {
    msg += F("<br>");
    msg += htmlEscape(detail);
  }
  msg += F("</div>");
  return msg;
}

static String buildConfigResultMessage(UiLang lang, const WeatherStation& ws, const String& title) {
  const auto& t = T(lang);
  const auto& c = ws.commLog();

  String msg;
  msg.reserve(900);
  msg += F("<div><strong>");
  msg += htmlEscape(title);
  msg += F(":</strong> ");
  msg += c.lastOk ? String(F("<span class='ok'>")) + t.statusOkShort + F("</span>") : String(F("<span class='bad'>")) + t.statusErrorShort + F("</span>");
  msg += F("<br>");
  msg += htmlEscape(t.modbusAction);
  msg += F(": ");
  msg += htmlEscape(c.lastAction.length() ? c.lastAction : String("-"));
  msg += F("<br>");
  msg += htmlEscape(t.modbusDuration);
  msg += F(": ");
  msg += String(c.lastDurationMs);
  msg += F(" ms");
  msg += F("<br>");
  msg += htmlEscape(t.modbusError);
  msg += F(": ");
  msg += (c.lastError.length() ? htmlEscape(c.lastError) : F("-"));
  msg += F("</div>");
  return msg;
}

static bool parseHex16(const String& s, uint16_t& out) {
  String v = s;
  v.trim();
  v.replace("0x", "");
  v.replace("0X", "");
  if (v.length() == 0 || v.length() > 4) return false;
  char* endptr = nullptr;
  unsigned long n = strtoul(v.c_str(), &endptr, 16);
  if (*endptr != '\0' || n > 0xFFFFUL) return false;
  out = static_cast<uint16_t>(n);
  return true;
}

static bool parseUint32Strict(const String& s, uint32_t& out) {
  String v = s;
  v.trim();
  if (v.length() == 0) return false;
  char* endptr = nullptr;
  unsigned long n = strtoul(v.c_str(), &endptr, 10);
  if (*endptr != '\0') return false;
  out = static_cast<uint32_t>(n);
  return true;
}

static bool parseHexBytesLoose(const String& input, uint8_t* out, size_t& outLen, size_t maxLen) {
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
  size_t bytes = s.length() / 2;
  if (bytes > maxLen) {
    outLen = 0;
    return false;
  }
  outLen = 0;
  for (size_t i = 0; i < bytes; i++) {
    String tok = s.substring(i * 2, i * 2 + 2);
    char* endptr = nullptr;
    unsigned long v = strtoul(tok.c_str(), &endptr, 16);
    if (*endptr != '\0' || v > 0xFFUL) {
      outLen = 0;
      return false;
    }
    out[outLen++] = static_cast<uint8_t>(v);
  }
  return true;
}

static bool writeRegisterToAddress(uint8_t slaveAddr, uint16_t reg, uint16_t value) {
  const uint8_t oldAddr = station.config().address;
  station.config().address = slaveAddr;
  const bool ok = station.writeRegister(reg, value);
  station.config().address = oldAddr;
  return ok;
}

static void handleRoot() {
  uiOnConfigPage = false;
  UiLang lang = currentLang();
  sendNoCacheHeaders();
  server.send(200, "text/html; charset=utf-8", buildMainPage(lang, station, ethConnected && ethGotIp, currentIp(), currentMac()));
}

static void handleConfig() {
  uiOnConfigPage = true;
  UiLang lang = currentLang();
  const String msg = flashMessage;
  flashMessage = String();
  sendNoCacheHeaders();
  server.send(200, "text/html; charset=utf-8", buildConfigPage(lang, station, msg));
}

static void handleLogo() {
  server.send_P(200, "image/png", reinterpret_cast<PGM_P>(logo_web_png), logo_web_png_len);
}

static void handleJson() {
  sendNoCacheHeaders();
  server.send(200, "application/json; charset=utf-8", buildMainDataJson(station, ethConnected && ethGotIp, currentIp(), currentMac()));
}

static bool applyRuntimeComm(UiLang lang, String& errorText, bool& changed) {
  changed = false;

  if (server.hasArg("address")) {
    uint32_t addr = 0;
    if (!parseUint32Strict(server.arg("address"), addr) || addr < 1 || addr > 247) {
      errorText = T(lang).address;
      return false;
    }
    if (station.config().address != static_cast<uint8_t>(addr)) {
      station.config().address = static_cast<uint8_t>(addr);
      changed = true;
    }
  }

  if (server.hasArg("baudrate")) {
    uint32_t baud = 0;
    if (!parseUint32Strict(server.arg("baudrate"), baud)) {
      errorText = T(lang).baudrate;
      return false;
    }
    switch (baud) {
      case 1200:
      case 2400:
      case 4800:
      case 9600:
      case 19200:
      case 38400:
      case 57600:
      case 115200:
        if (station.config().baudrate != baud) {
          station.setBaudrate(baud);
          changed = true;
        }
        break;
      default:
        errorText = T(lang).baudrate;
        return false;
    }
  }

  if (changed) savePreferences();
  return true;
}

static void handleConfigSave() {
  UiLang lang = currentLang();
  const auto& t = T(lang);

  String runtimeError;
  bool runtimeChanged = false;
  if (!applyRuntimeComm(lang, runtimeError, runtimeChanged)) {
    redirectWithMessage("/config", lang, buildSimpleMessage(lang, t.save, runtimeError, false));
    return;
  }

  bool didWrite = false;
  bool ok = true;
  String firstValidationError;

  const bool hasWindOffset = server.hasArg("windOffset") && server.arg("windOffset").length() > 0;
  const bool hasRainSensitivity = server.hasArg("rainSensitivity") && server.arg("rainSensitivity").length() > 0;

  if (hasWindOffset) {
    uint16_t v = 0;
    if (!parseHex16(server.arg("windOffset"), v)) {
      ok = false;
      firstValidationError = t.windOffset;
    } else {
      ok = station.writeRegister(REG_CFG_WIND_DIR_OFFSET, v) && ok;
      station.pushConfigLog();
      didWrite = true;
      if (ok) station.config().windOffset = v;
    }
  }

  if (hasRainSensitivity) {
    uint16_t v = 0;
    if (!parseHex16(server.arg("rainSensitivity"), v)) {
      ok = false;
      if (!firstValidationError.length()) firstValidationError = t.rainSensitivity;
    } else {
      ok = station.writeRegister(REG_CFG_RAIN_SENSITIVITY, v) && ok;
      station.pushConfigLog();
      didWrite = true;
      if (ok) station.config().rainSensitivity = v;
    }
  }

  if (!didWrite) {
    const String detail = runtimeChanged ? String() : firstValidationError;
    redirectWithMessage("/config", lang, buildSimpleMessage(lang, t.save, detail, runtimeChanged || ok));
    return;
  }

  if (!ok && firstValidationError.length()) {
    String msg = buildConfigResultMessage(lang, station, t.save);
    msg += F("<div style='margin-top:8px'>");
    msg += htmlEscape(firstValidationError);
    msg += F("</div>");
    redirectWithMessage("/config", lang, msg);
    return;
  }

  redirectWithMessage("/config", lang, buildConfigResultMessage(lang, station, t.save));
}

static void handleCmdWindZero() {
  UiLang lang = currentLang();
  station.commandZeroWind();
  station.pushConfigLog();
  redirectWithMessage("/config", lang, buildConfigResultMessage(lang, station, T(lang).commandWindZero));
}

static void handleCmdRainZero() {
  UiLang lang = currentLang();
  station.commandZeroRain();
  station.pushConfigLog();
  redirectWithMessage("/config", lang, buildConfigResultMessage(lang, station, T(lang).commandRainZero));
}

static void handleCmdManual() {
  UiLang lang = currentLang();
  const auto& t = T(lang);
  if (!server.hasArg("address") || !server.hasArg("reg") || !server.hasArg("value")) {
    redirectWithMessage("/config", lang, buildSimpleMessage(lang, t.manualCommand, t.writeFail, false));
    return;
  }

  uint32_t addrParsed = 0;
  uint16_t regIn = 0;
  uint16_t valIn = 0;
  if (!parseUint32Strict(server.arg("address"), addrParsed) || addrParsed < 1 || addrParsed > 247 ||
      !parseHex16(server.arg("reg"), regIn) || !parseHex16(server.arg("value"), valIn)) {
    redirectWithMessage("/config", lang, buildSimpleMessage(lang, t.manualCommand, t.writeFail, false));
    return;
  }

  writeRegisterToAddress(static_cast<uint8_t>(addrParsed), regIn, valIn);
  station.pushConfigLog();
  redirectWithMessage("/config", lang, buildConfigResultMessage(lang, station, t.manualCommand));
}

static void handleCmdRawHex() {
  UiLang lang = currentLang();
  const auto& t = T(lang);
  if (!server.hasArg("rawhex")) {
    redirectWithMessage("/config", lang, buildSimpleMessage(lang, t.rawHexTitle, t.writeFail, false));
    return;
  }

  uint8_t frame[128];
  size_t frameLen = 0;
  if (!parseHexBytesLoose(server.arg("rawhex"), frame, frameLen, sizeof(frame))) {
    redirectWithMessage("/config", lang, buildSimpleMessage(lang, t.rawHexTitle, t.writeFail, false));
    return;
  }

  uint8_t resp[128] = {0};
  station.sendRawFrame(frame, frameLen, resp, sizeof(resp), 300, String(t.rawHexTitle));
  station.pushConfigLog();
  redirectWithMessage("/config", lang, buildConfigResultMessage(lang, station, t.rawHexTitle));
}

static void handleNotFound() {
  sendNoCacheHeaders();
  server.send(404, "text/plain; charset=utf-8", "404 Not Found");
}

static void beginEthernet() {
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
}

static void beginWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/data.json", HTTP_GET, handleJson);
  server.on("/logo.png", HTTP_GET, handleLogo);
  server.on("/config-save", HTTP_POST, handleConfigSave);
  server.on("/cmd-wind-zero", HTTP_POST, handleCmdWindZero);
  server.on("/cmd-rain-zero", HTTP_POST, handleCmdRainZero);
  server.on("/cmd-manual", HTTP_POST, handleCmdManual);
  server.on("/cmd-rawhex", HTTP_POST, handleCmdRawHex);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("ESPlan Weather Station start");
  fwLog(F("Boot"));

  loadPreferences();
  station.begin();
  station.readSensorConfig();
  station.readAll();

  beginEthernet();
  beginWeb();
  fwLog(F("HTTP server started"));
}

void loop() {
  server.handleClient();

  const unsigned long now = millis();
  if (!uiOnConfigPage && (now - lastPollMs >= SENSOR_POLL_MS)) {
    lastPollMs = now;
    const bool ok = station.readAll();
    if (!ok) {
      fwLog(String(F("Modbus read failed: ")) + station.commLog().lastError);
    }
  }

  if (!uiOnConfigPage && (now - lastDetectRecheckMs >= DETECT_RECHECK_MS)) {
    lastDetectRecheckMs = now;
    const bool ok = station.readSensorConfig();
    if (!ok) {
      fwLog(String(F("Sensor config refresh failed: ")) + station.commLog().lastError);
    }
  }
}
