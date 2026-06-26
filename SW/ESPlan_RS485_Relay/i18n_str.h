#ifndef RL_I18N_STR_H
#define RL_I18N_STR_H
/*
 * i18n_str.h – Preklady webove rozhrani (CZ / EN)
 * pro 8/16-kanalovy RS485 rele modul
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
  const char* sectionRelays;
  const char* sectionActions;
  const char* sectionAdvanced;
  const char* colChannel;
  const char* colName;
  const char* colState;
  const char* colCtrl;
  const char* stateOn;
  const char* stateOff;
  const char* btnOn;
  const char* btnOff;
  const char* btnToggle;
  const char* btnLatch;
  const char* btnMomentary;
  const char* btnDelay;
  const char* btnAllOn;
  const char* btnAllOff;
  const char* commError;
  const char* lastAction;
  const char* labelDelay;
  const char* labelDelaySec;
  const char* labelLatchHelp;
  const char* labelMomentaryHelp;
  const char* labelDelayHelp;

  // Konfigurace
  const char* pageConfigTitle;
  const char* sectionComm;
  const char* cfgAddr;
  const char* cfgAddrHelp;
  const char* cfgBaud;
  const char* cfgBaudHelp;
  const char* cfgChannels;
  const char* cfgChannelsHelp;
  const char* cfgDefDelay;
  const char* cfgDefDelayHelp;

  const char* sectionNames;
  const char* cfgNameHelp;

  const char* sectionInfo;
  const char* infoDip;

  const char* btnSave;

  const char* sectionRaw;
};

// ─── Cestina ──────────────────────────────────────────────────────────────────
static const I18nStrings STR_CZ = {
  "ESPlan RS485 rele",
  "Ovladani",
  "Konfigurace",
  "English",

  "Ovladani rele",
  "Stav kanalu",
  "Hromadne ovladani",
  "Specialni rezimy",
  "#",
  "Nazev",
  "Stav",
  "Akce",
  "ON",
  "OFF",
  "ON",
  "OFF",
  "Toggle",
  "Latch",
  "Pulse",
  "Delay",
  "Vsechny ON",
  "Vsechny OFF",
  "&#9888; Chyba komunikace",
  "Posledni akce",
  "Delay (sekundy)",
  "s",
  "Latch (inter-locking) sepne jen vybrany kanal a ostatni rozepne.",
  "Pulse (momentary) sepne kanal na 1 sekundu, pak automaticky rozepne.",
  "Delay sepne kanal, po zadanem case ho automaticky rozepne (0-255 s).",

  "Konfigurace",
  "Komunikacni parametry",
  "Modbus adresa",
  "Adresa modulu nastavena DIP prepinaci A0-A5 na desce (0-47).",
  "Prenosova rychlost",
  "Baudrate (nastavi se HW pady M1/M2): 9600 (default), 2400, 4800, 19200.",
  "Pocet kanalu",
  "8-kanalova nebo 16-kanalova varianta modulu.",
  "Vychozi Delay",
  "Pocatecni hodnota Delay tlacitka (0-255 s).",

  "Nazvy kanalu",
  "Pojmenovani jednotlivych kanalu (max 15 znaku)",

  "Informace o zarizeni",
  "Adresa Modbus modulu se nastavuje DIP prepinaci A0-A5 (binarne, 6 bitu). "
  "Baudrate se nastavi propojkami M1/M2: 9600 / 2400 / 4800 / 19200. "
  "Modul take podporuje AT prikazy pres UART, pokud zkratite pady M0 - v tom rezimu "
  "Slave ID nepouziva, a tento ESP32 program ho neumi (pouziva pouze Modbus-RTU).",

  "Ulozit konfiguraci",

  "RS485 komunikace (raw)"
};

// ─── Anglictina ───────────────────────────────────────────────────────────────
static const I18nStrings STR_EN = {
  "ESPlan RS485 relay",
  "Control",
  "Configuration",
  "Cesky",

  "Relay control",
  "Channel state",
  "Bulk control",
  "Advanced modes",
  "#",
  "Name",
  "State",
  "Action",
  "ON",
  "OFF",
  "ON",
  "OFF",
  "Toggle",
  "Latch",
  "Pulse",
  "Delay",
  "All ON",
  "All OFF",
  "&#9888; Communication error",
  "Last action",
  "Delay (seconds)",
  "s",
  "Latch (inter-locking) turns ON only the selected channel and turns OFF all others.",
  "Pulse (momentary) closes the channel for 1 second, then opens automatically.",
  "Delay closes the channel and reopens it after the given time (0-255 s).",

  "Configuration",
  "Communication Settings",
  "Modbus address",
  "Module address is set by DIP switches A0-A5 on the board (0-47).",
  "Baud rate",
  "Baud rate (set by HW pads M1/M2): 9600 (default), 2400, 4800, 19200.",
  "Channel count",
  "8-channel or 16-channel variant.",
  "Default Delay",
  "Initial value for the Delay button (0-255 s).",

  "Channel names",
  "Name each channel (max 15 chars)",

  "Device info",
  "Module Modbus address is set by DIP switches A0-A5 (6-bit binary). "
  "Baud rate is selected by jumper pads M1/M2: 9600 / 2400 / 4800 / 19200. "
  "The module also supports AT commands over UART when M0 pads are shorted - "
  "Slave ID is then unused, and this firmware does not use that mode (Modbus-RTU only).",

  "Save configuration",

  "RS485 Communication (raw)"
};

inline const I18nStrings& getStrings(bool isCz) {
  return isCz ? STR_CZ : STR_EN;
}

#endif // RL_I18N_STR_H
