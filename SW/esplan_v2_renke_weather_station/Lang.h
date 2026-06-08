#pragma once

#include <Arduino.h>

struct Texts {
  const char* appTitle;
  const char* tabMain;
  const char* tabConfig;
  const char* language;
  const char* sensorStatus;
  const char* values;
  const char* present;
  const char* absent;
  const char* pollAge;
  const char* seconds;
  const char* windSpeed;
  const char* beaufort;
  const char* windDir8;
  const char* windDirDeg;
  const char* humidity;
  const char* temperature;
  const char* noise;
  const char* pressure;
  const char* lightHigh;
  const char* lightLow;
  const char* lightLux100;
  const char* rain;
  const char* compass;
  const char* solarRadiation;
  const char* sensorWind;
  const char* sensorHumidityTemp;
  const char* sensorNoise;
  const char* sensorAir;
  const char* sensorPressure;
  const char* sensorLight;
  const char* sensorRain;
  const char* sensorCompass;
  const char* sensorSolar;
  const char* configTitle;
  const char* address;
  const char* baudrate;
  const char* save;
  const char* writeOk;
  const char* writeFail;
  const char* execute;
  const char* windOffset;
  const char* rainSensitivity;
  const char* commandWindZero;
  const char* commandRainZero;
  const char* tableItem;
  const char* tableSetting;
  const char* tableNote;
  const char* modbusLog;
  const char* modbusFrames;
  const char* modbusLastStatus;
  const char* modbusAction;
  const char* modbusDuration;
  const char* modbusCounters;
  const char* modbusError;
  const char* modbusTx;
  const char* modbusRx;
  const char* noteAddress;
  const char* noteBaudrate;
  const char* noteWindOffset;
  const char* noteRainSensitivity;
  const char* noteWindZero;
  const char* noteRainZero;
  const char* rawRegisters;
  const char* manualCommand;
  const char* manualAddress;
  const char* manualRegisterHex;
  const char* manualValueHex;
  const char* manualSend;
  const char* manualPreview;
  const char* manualNote;
  const char* rawHexTitle;
  const char* rawHexInput;
  const char* rawHexSend;
  const char* rawHexNote;
  const char* noResult;
  const char* statusOkShort;
  const char* statusErrorShort;
};

enum class UiLang : uint8_t {
  CZ = 0,
  EN = 1
};

inline UiLang parseLang(const String& v) {
  String s = v;
  s.toLowerCase();
  return (s == "en") ? UiLang::EN : UiLang::CZ;
}

inline const char* langCode(UiLang lang) {
  return (lang == UiLang::EN) ? "en" : "cz";
}

inline const Texts& T(UiLang lang) {
  static const Texts cz = {
    "ESPlan Meteostanice RS-FSXCS-N01",
    "Hlavní stránka",
    "Konfigurace",
    "Jazyk",
    "Stav čidel",
    "Aktuální hodnoty",
    "Přítomno",
    "Nepřítomno",
    "Stáří dat",
    "s",
    "Rychlost větru",
    "Síla větru",
    "Směr větru 0-7",
    "Směr větru",
    "Vlhkost",
    "Teplota",
    "Hluk",
    "Tlak",
    "Osvětlení high",
    "Osvětlení low",
    "Osvětlení",
    "Déšť",
    "Kompas",
    "Sluneční radiace",
    "Vítr",
    "Vlhkost / Teplota",
    "Hluk",
    "Kvalita vzduchu",
    "Tlak",
    "Světlo",
    "Déšť",
    "Kompas",
    "Solární",
    "Konfigurace snímače",
    "Adresa (ESP32)",
    "Baudrate (ESP32)",
    "Uložit",
    "Zápis OK",
    "Zápis CHYBA",
    "Provést",
    "Offset směru větru",
    "Citlivost optického deště",
    "Nulování větru",
    "Nulování deště",
    "Položka",
    "Nastavení",
    "Poznámka",
    "Modbus log",
    "Modbus rámce",
    "Stav",
    "Akce",
    "Doba",
    "Počítadla",
    "Chyba",
    "TX",
    "RX",
    "Toto nastavení mění pouze komunikaci ESP32, ne konfiguraci samotného snímače.",
    "Toto nastavení mění pouze komunikaci ESP32, ne konfiguraci samotného snímače.",
    "Registr 0x6000, HEX hodnota.",
    "Registr 0x6003, HEX hodnota.",
    "Zapíše kalibrační příkaz do snímače.",
    "Zapíše nulovací příkaz do snímače.",
    "Surové registry",
    "Ruční Modbus příkaz",
    "Adresa",
    "Registr (HEX)",
    "Hodnota (HEX)",
    "Odeslat",
    "Rámec s CRC",
    "Odešle se funkce 0x06. Adresa je decimálně, registr a hodnota jsou HEX. CRC se počítá automaticky.",
    "Celý HEX příkaz",
    "HEX rámec",
    "Odeslat HEX",
    "Zadej celý rámec v HEX bez CRC(dopočítá se automaticky). Neověřuje se správnost rámce, odesílá se tak jak je zadán.",
    "Zatím bez výsledku",
    "OK",
    "CHYBA"
  };

  static const Texts en = {
    "ESPlan Weather Station RS-FSXCS-N01",
    "Main page",
    "Configuration",
    "Language",
    "Sensor status",
    "Current values",
    "Present",
    "Absent",
    "Data age",
    "s",
    "Wind speed",
    "Beaufort",
    "Wind direction 0-7",
    "Wind direction",
    "Humidity",
    "Temperature",
    "Noise",
    "Pressure",
    "Light high",
    "Light low",
    "Light",
    "Rain",
    "Compass",
    "Solar radiation",
    "Wind",
    "Humidity / Temperature",
    "Noise",
    "Air quality",
    "Pressure",
    "Light",
    "Rain",
    "Compass",
    "Solar",
    "Sensor configuration",
    "Address (ESP32)",
    "Baudrate (ESP32)",
    "Save",
    "Write OK",
    "Write FAIL",
    "Execute",
    "Wind direction offset",
    "Optical rain sensitivity",
    "Zero wind",
    "Zero rain",
    "Item",
    "Setting",
    "Note",
    "Modbus log",
    "Modbus frames",
    "Status",
    "Action",
    "Duration",
    "Counters",
    "Error",
    "TX",
    "RX",
    "This setting affects only ESP32 communication, not the sensor configuration itself.",
    "This setting affects only ESP32 communication, not the sensor configuration itself.",
    "Register 0x6000, HEX value.",
    "Register 0x6003, HEX value.",
    "Writes the calibration command to the sensor.",
    "Writes the reset command to the sensor.",
    "Raw registers",
    "Manual Modbus command",
    "Address",
    "Register (HEX)",
    "Value (HEX)",
    "Send",
    "Frame with CRC",
    "Function 0x06 will be sent. Address is decimal, register and value are HEX. CRC is calculated automatically.",
    "Full HEX command",
    "HEX frame",
    "Send HEX",
    "Enter the entire frame in HEX without CRC (it will be calculated automatically). The frame is not verified for correctness, it is sent as entered.",
    "No result yet",
    "OK",
    "ERROR"
  };

  return (lang == UiLang::EN) ? en : cz;
}
