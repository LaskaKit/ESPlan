#ifndef WS_I18N_STR_H
#define WS_I18N_STR_H
/*
 * i18n_str.h – Preklady webove rozhrani (CZ / EN)
 * pro snimac ZTS-3000-FSJT-N01
 */

#include <Arduino.h>

struct I18nStrings {
  // Navigace
  const char* title;
  const char* navStatus;
  const char* navConfig;
  const char* langSwitch;

  // Stavova stranka
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
  const char* btnDetect;

  // Nazvy mereni
  const char* snsWind;
  const char* snsWindSpeed;
  const char* snsWindLevel;
  const char* snsWindLevelName;
  const char* unitMs;
  const char* unitBft;

  // Konfigurace
  const char* pageConfigTitle;
  const char* sectionComm;
  const char* cfgAddr;
  const char* cfgAddrHelp;
  const char* cfgBaud;
  const char* cfgBaudHelp;
  const char* cfgPoll;
  const char* cfgPollHelp;

  const char* sectionActions;
  const char* btnChangeAddr;
  const char* btnChangeAddrHelp;
  const char* cfgNewAddr;

  const char* btnSave;

  const char* sectionRaw;
};

// ─── Cestina ──────────────────────────────────────────────────────────────────
static const I18nStrings STR_CZ = {
  "ESPlan ZTS-3000 Anemometr",
  "Stav snimace",
  "Konfigurace",
  "English",

  "Stav a hodnoty snimace",
  "Detekce snimace",
  "Namerene hodnoty",
  "Velicina",
  "Pritomnost",
  "Hodnota",
  "Jednotka",
  "&#10003; Pritomno",
  "&#8212; Nepritomno",
  "Posledni aktualizace",
  "&mdash;",
  "&#9888; Chyba komunikace",
  "Znovu detekovat",

  "Anemometr ZTS-3000",
  "Rychlost vetru",
  "Sila vetru (Beaufort)",
  "Slovni popis",
  "m/s",
  "Bft",

  "Konfigurace snimace",
  "Komunikacni parametry",
  "Modbus adresa",
  "Aktualni adresa snimace na sbernici RS485 (1-247, vychozi: 1)",
  "Prenosova rychlost",
  "Baudrate RS485 (datasheet: 4800 bps)",
  "Interval cteni",
  "Interval periodickeho cteni dat v sekundach (1-60)",

  "Akce snimace",
  "Zmenit adresu snimace",
  "Zapise novou adresu do reg. 0x07D0 snimace (FC06). Pote upravte adresu vyse.",
  "Nova adresa",

  "Ulozit konfiguraci",

  "RS485 komunikace (raw)"
};

// ─── Anglictina ───────────────────────────────────────────────────────────────
static const I18nStrings STR_EN = {
  "ESPlan ZTS-3000 Anemometer",
  "Sensor Status",
  "Configuration",
  "Cesky",

  "Sensor Status &amp; Readings",
  "Sensor Detection",
  "Live Readings",
  "Quantity",
  "Presence",
  "Value",
  "Unit",
  "&#10003; Present",
  "&#8212; Not present",
  "Last update",
  "&mdash;",
  "&#9888; Communication error",
  "Re-detect sensor",

  "ZTS-3000 anemometer",
  "Wind speed",
  "Wind force (Beaufort)",
  "Description",
  "m/s",
  "Bft",

  "Sensor Configuration",
  "Communication Settings",
  "Modbus address",
  "Current sensor address on the RS485 bus (1-247, default: 1)",
  "Baud rate",
  "RS485 baud rate (datasheet: 4800 bps)",
  "Poll interval",
  "Data polling interval in seconds (1-60)",

  "Sensor Actions",
  "Change sensor address",
  "Writes the new address into register 0x07D0 (FC06). Update the address above afterwards.",
  "New address",

  "Save configuration",

  "RS485 Communication (raw)"
};

inline const I18nStrings& getStrings(bool isCz) {
  return isCz ? STR_CZ : STR_EN;
}

#endif // WS_I18N_STR_H
