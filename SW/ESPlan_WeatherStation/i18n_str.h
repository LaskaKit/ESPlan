#ifndef WS_I18N_STR_H
#define WS_I18N_STR_H
/*
 * i18n_str.h – Překlady webového rozhraní (CZ / EN)
 */

#include <Arduino.h>

struct I18nStrings {
  // Navigace
  const char* title;
  const char* navStatus;
  const char* navConfig;
  const char* langSwitch;

  // Stavová stránka
  const char* pageStatusTitle;
  const char* sectionSensors;
  const char* sectionValues;
  const char* colSensor;
  const char* colPresent;
  const char* colValue;
  const char* colUnit;
  const char* yes;
  const char* no;
  const char* lastUpdate;
  const char* noData;
  const char* commError;
  const char* btnRefresh;
  const char* btnDetect;
  const char* autoRefresh;

  // Názvy senzorů
  const char* snsWind;
  const char* snsWindSpeed;
  const char* snsWindForce;
  const char* snsWindDir;
  const char* snsWindDir360;
  const char* snsTempHum;
  const char* snsTemp;
  const char* snsHum;
  const char* snsPressure;
  const char* snsNoise;
  const char* snsPM25;
  const char* snsPM10;
  const char* snsCO2;
  const char* snsLight;
  const char* snsRainfall;
  const char* snsCompass;
  const char* snsSolar;

  // Konfigurace
  const char* pageConfigTitle;
  const char* sectionComm;
  const char* cfgAddr;
  const char* cfgAddrHelp;
  const char* cfgBaud;
  const char* cfgBaudHelp;
  const char* cfgPoll;
  const char* cfgPollHelp;

  const char* sectionCalib;
  const char* cfgWindDirOffset;
  const char* cfgWindDirOffsetHelp;
  const char* cfgWindDirOff0;
  const char* cfgWindDirOff1;
  const char* cfgRainSens;
  const char* cfgRainSensHelp;
  const char* cfgSlot507;
  const char* cfgSlot507Help;
  const char* cfgSlotAuto;
  const char* cfgSlotPM;
  const char* cfgSlotCO2;

  const char* sectionActions;
  const char* btnWindZero;
  const char* btnWindZeroHelp;
  const char* btnRainZero;
  const char* btnRainZeroHelp;

  const char* btnSave;

  const char* sectionRaw;

  const char* windDirNames[8];
};

// ─── Čeština ──────────────────────────────────────────────────────────────────
static const I18nStrings STR_CZ = {
  "ESPlan Meteostanice",
  "Stav cidel",
  "Konfigurace",
  "English",

  "Stav a hodnoty snimacu",
  "Detekce senzoru",
  "Namerene hodnoty",
  "Senzor",
  "Pritomnost",
  "Hodnota",
  "Jednotka",
  "&#10003; Pritomno",
  "&#8212; Nepritomno",
  "Posledni aktualizace",
  "&mdash;",
  "&#9888; Chyba komunikace",
  "Obnovit",
  "Znovu detekovat",
  "Auto obnoveni",

  "Vitr",
  "Rychlost vetru",
  "Sila vetru (Beaufort)",
  "Smer vetru (0-7)",
  "Smer vetru (0-360&deg;)",
  "Teplota / Vlhkost",
  "Teplota",
  "Relativni vlhkost",
  "Atmosfericky tlak",
  "Hladina hluku",
  "Prach PM2.5",
  "Prach PM10",
  "CO&#8322;",
  "Osvit",
  "Srazky",
  "El. kompas",
  "Celkove sol. zareni",

  "Konfigurace snimacu",
  "Komunikacni parametry",
  "Adresa Modbus",
  "Adresa snimacu na sbernici RS485 (1-247, vychozi: 1)",
  "Prenosova rychlost",
  "Baudrate RS485 (vychozi: 4800 bps)",
  "Interval cteni",
  "Interval periodickeho cteni dat v sekundach (1-60)",

  "Kalibrace a nastaveni",
  "Korekce smeru vetru",
  "Otoceni smeru o 180 stupnu",
  "Normalni (0&deg;)",
  "Otoceni 180&deg;",
  "Citlivost destoveho snimacu",
  "Hex hodnota registru 0x6003 (vychozi: 0x11). Mensi = citlivejsi.",
  "Typ snimacu v registrech 507/508",
  "Auto detekce nebo rucni urceni PM2.5/PM10 nebo CO2",
  "Automaticka detekce",
  "PM2.5 + PM10",
  "CO&#8322;",

  "Akce snimacu",
  "Vynulovat rychlost vetru",
  "Zapise 0xAA do reg. 0x6001. Pockejte 10 s.",
  "Vynulovat srazkomery",
  "Zapise 0x5A do reg. 0x6002.",

  "Ulozit konfiguraci",

  "RS485 komunikace (raw)",

  {"S", "SV", "V", "JV", "J", "JZ", "Z", "SZ"}
};

// ─── Angličtina ───────────────────────────────────────────────────────────────
static const I18nStrings STR_EN = {
  "ESPlan Weather Station",
  "Sensor Status",
  "Configuration",
  "Cesky",

  "Sensor Status &amp; Readings",
  "Sensor Detection",
  "Live Readings",
  "Sensor",
  "Presence",
  "Value",
  "Unit",
  "&#10003; Present",
  "&#8212; Not present",
  "Last update",
  "&mdash;",
  "&#9888; Communication error",
  "Refresh",
  "Re-detect sensors",
  "Auto refresh",

  "Wind",
  "Wind speed",
  "Wind force (Beaufort)",
  "Wind direction (0-7)",
  "Wind direction (0-360&deg;)",
  "Temperature / Humidity",
  "Temperature",
  "Relative humidity",
  "Atmospheric pressure",
  "Noise level",
  "Particulate PM2.5",
  "Particulate PM10",
  "CO&#8322;",
  "Illuminance",
  "Rainfall",
  "E-compass",
  "Total solar radiation",

  "Sensor Configuration",
  "Communication Settings",
  "Modbus Address",
  "Sensor address on RS485 bus (1-247, default: 1)",
  "Baud Rate",
  "RS485 baud rate (default: 4800 bps)",
  "Poll Interval",
  "Data reading interval in seconds (1-60)",

  "Calibration &amp; Settings",
  "Wind direction offset",
  "Rotate direction by 180 degrees",
  "Normal (0&deg;)",
  "Rotated 180&deg;",
  "Rain sensor sensitivity",
  "Hex value of register 0x6003 (default: 0x11). Lower = more sensitive.",
  "Sensor type in registers 507/508",
  "Auto-detect or manually specify PM2.5/PM10 or CO2",
  "Auto detect",
  "PM2.5 + PM10",
  "CO&#8322;",

  "Sensor Actions",
  "Zero wind speed",
  "Writes 0xAA to reg 0x6001. Wait 10 s.",
  "Reset rainfall counter",
  "Writes 0x5A to reg 0x6002.",

  "Save configuration",

  "RS485 Communication (raw)",

  {"N", "NE", "E", "SE", "S", "SW", "W", "NW"}
};

inline const I18nStrings& getStrings(bool isCz) {
  return isCz ? STR_CZ : STR_EN;
}

#endif // WS_I18N_STR_H
