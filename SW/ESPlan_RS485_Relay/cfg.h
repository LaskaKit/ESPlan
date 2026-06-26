#ifndef RL_CFG_H
#define RL_CFG_H
/*
 * cfg.h – Konfigurace HW a datove struktury
 *
 * Zarizeni: 8 / 16-kanalovy RS485 rele modul (Modbus-RTU)
 *           https://www.laskakit.cz/8-kanalu-12v-rele-modul-250vac-10a--rs485--din/
 *
 * Protokol (datasheet 8ch_rele_rs485_protocols.pdf):
 *   FC 0x06 (Write Single Register), 8N1
 *   Adresa: 0x00-0x2F (0-47) – nastavena DIP prepinaci A0-A5 (HW)
 *   Baudrate: 9600 / 2400 / 4800 / 19200 (vychozi 9600, nastavi se M1/M2 pady)
 *
 *   Jednotlivy kanal (reg 0x0001 .. 0x000N):
 *     val_hi = funkce, val_lo = parametr/delay
 *
 *     0x0100   Open       (sepne)
 *     0x0200   Close      (rozepne)
 *     0x0300   Toggle     (self-locking)
 *     0x0400   Latch      (inter-locking; jen tento kanal ON, ostatni OFF)
 *     0x0500   Momentary  (non-locking, 1s puls)
 *     0x06xx   Delay XX s (xx = delka v sekundach 0x00..0xFF)
 *
 *   Vsechny kanaly (reg 0x0000):
 *     0x0700   All ON
 *     0x0800   All OFF
 *
 *   Cteni stavu (FC 0x03):
 *     reg 0x0001 .. 0x000N, vrati 0x0001 (ON) / 0x0000 (OFF)
 *
 *   Adresa zarizeni se NEMENI Modbusem – nastavi DIP prepinacem!
 */

#include <Arduino.h>
#include <Preferences.h>

// ─── ETH (LAN8720 na ESPlan) ──────────────────────────────────────────────────
#define ETH_PHY_TYPE    ETH_PHY_LAN8720
#define ETH_PHY_ADDR    0
#define ETH_PHY_MDC     23
#define ETH_PHY_MDIO    18
#define ETH_PHY_POWER   -1
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT

// ─── RS485 / UART ─────────────────────────────────────────────────────────────
#define PIN_RS485_RX    36
#define PIN_RS485_TX    4

// ─── Modbus vychozi hodnoty ───────────────────────────────────────────────────
#define DEFAULT_MODBUS_ADDR     1
#define DEFAULT_BAUD_RATE       9600
#define DEFAULT_CHANNEL_COUNT   8        // 8 nebo 16
#define MAX_CHANNEL_COUNT       16
#define MODBUS_ADDR_MIN         0
#define MODBUS_ADDR_MAX         47       // 0x2F dle datasheetu (DIP A0-A5)

// ─── Registry rele modulu ────────────────────────────────────────────────────
#define REG_ALL_CHANNELS    0x0000
#define REG_CHANNEL_BASE    0x0001       // reg 0x0001 = ch1, az 0x0010 = ch16

// ─── Funkcni kody (val_hi) ───────────────────────────────────────────────────
#define FN_OPEN          0x01
#define FN_CLOSE         0x02
#define FN_TOGGLE        0x03
#define FN_LATCH         0x04
#define FN_MOMENTARY     0x05
#define FN_DELAY         0x06     // val_lo = delka v s (0-255)
#define FN_ALL_ON        0x07     // jen s reg 0x0000
#define FN_ALL_OFF       0x08     // jen s reg 0x0000

// helper na slozeni val pole pro Modbus zapis
inline uint16_t makeVal(uint8_t fn, uint8_t param = 0) {
  return ((uint16_t)fn << 8) | param;
}

// ─── Stav rele modulu ────────────────────────────────────────────────────────
struct RelayState {
  bool     channels[MAX_CHANNEL_COUNT] = { false };  // SW shadow (true = ON)
  uint32_t lastUpdateMs                = 0;
  uint32_t lastCmdMs                   = 0;
  bool     commError                   = false;
  String   errorMsg                    = "";
  String   lastAction                  = "";   // pro UI
};

// ─── Konfigurace (uklada se do NVS) ──────────────────────────────────────────
struct RelayConfig {
  uint8_t  modbusAddr    = DEFAULT_MODBUS_ADDR;
  uint32_t baudRate      = DEFAULT_BAUD_RATE;
  uint8_t  channelCount  = DEFAULT_CHANNEL_COUNT;   // 8 nebo 16
  uint8_t  defaultDelay  = 10;                      // pro tlacitko Delay (sekundy)

  // Nazvy kanalu (max 15 znaku + \0)
  char     names[MAX_CHANNEL_COUNT][16] = {
    "Rele 1",  "Rele 2",  "Rele 3",  "Rele 4",
    "Rele 5",  "Rele 6",  "Rele 7",  "Rele 8",
    "Rele 9",  "Rele 10", "Rele 11", "Rele 12",
    "Rele 13", "Rele 14", "Rele 15", "Rele 16"
  };
};

// ─── NVS load / save ─────────────────────────────────────────────────────────
inline void loadConfig(Preferences &p, RelayConfig &c) {
  c.modbusAddr   = p.getUChar("mbAddr",   DEFAULT_MODBUS_ADDR);
  c.baudRate     = p.getUInt ("baud",     DEFAULT_BAUD_RATE);
  c.channelCount = p.getUChar("chCount",  DEFAULT_CHANNEL_COUNT);
  if (c.channelCount != 8 && c.channelCount != 16) c.channelCount = 8;
  c.defaultDelay = p.getUChar("defDelay", 10);

  for (uint8_t i = 0; i < MAX_CHANNEL_COUNT; i++) {
    char key[8];
    snprintf(key, sizeof(key), "n%u", i);
    String def = String("Rele ") + (i + 1);
    String v   = p.getString(key, def);
    strncpy(c.names[i], v.c_str(), 15);
    c.names[i][15] = 0;
  }
}

inline void saveConfig(Preferences &p, const RelayConfig &c) {
  p.putUChar("mbAddr",   c.modbusAddr);
  p.putUInt ("baud",     c.baudRate);
  p.putUChar("chCount",  c.channelCount);
  p.putUChar("defDelay", c.defaultDelay);
  for (uint8_t i = 0; i < MAX_CHANNEL_COUNT; i++) {
    char key[8];
    snprintf(key, sizeof(key), "n%u", i);
    p.putString(key, c.names[i]);
  }
}

#endif // RL_CFG_H
