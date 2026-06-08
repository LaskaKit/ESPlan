#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "Lang.h"
#include "WeatherStation.h"

String htmlEscape(const String& in);
String fmtFloat(float v, uint8_t decimals = 1);
String buildMainPage(UiLang lang, const WeatherStation& ws, bool ethUp, const String& ip, const String& mac);
String buildConfigPage(UiLang lang, const WeatherStation& ws, const String& flashMsg);
String buildMainDataJson(const WeatherStation& ws, bool ethUp, const String& ip, const String& mac);
String buildStyle(const Texts& t, UiLang lang);
