#ifndef WS_CFG_H
#define WS_CFG_H
/*
 * cfg.h – Konfigurace HW a datove struktury
 *
 * Snimac: ZTS-3000-FSJT-N01 (polykarbonatovy 3-pohárový anemometr)
 *  - RS485, Modbus-RTU, 8N1, vychozi 4800 bps
 *  - Vychozi adresa: 0x01 (1)
 *  - 2 vstupni registry (FC 0x03):
 *      reg 0x0000 = rychlost vetru x10  (napr. 0x0024 = 36 = 3.6 m/s)
 *      reg 0x0001 = sila vetru (Beaufort 0-17)
 *  - Zmena adresy: zapis (FC 0x06) do reg 0x07D0
 *
 * POZOR: Konfigurace HW pinu je pevna (nelze menit pres web).
 *        Pres webove rozhrani lze menit pouze komunikacni parametry
 *        a adresu snimace.
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
// DE pin je rizen hardwarove – nepotrebujeme softwarove ovladani

// ─── Modbus vychozi hodnoty ───────────────────────────────────────────────────
#define DEFAULT_MODBUS_ADDR     1
#define DEFAULT_BAUD_RATE       4800     // datasheet ZTS-3000-FSJT-N01
#define DEFAULT_POLL_INTERVAL   5        // sekundy

// ─── Modbus registry snimace (ZTS-3000-FSJT-N01) ─────────────────────────────
// Cteni (FC03):
#define REG_WIND_SPEED      0x0000   // rychlost vetru * 10  (m/s)
#define REG_WIND_LEVEL      0x0001   // sila vetru (Beaufort 0-17)
#define REG_BLOCK_COUNT     2        // celkem 2 registry

// Zapis (FC06):
#define REG_DEVICE_ADDR     0x07D0   // adresa snimace (1-247)

// ─── Flag pritomnosti snimace ────────────────────────────────────────────────
struct SensorPresence {
  bool wind = false;
};

// ─── Namerene hodnoty ─────────────────────────────────────────────────────────
struct SensorData {
  SensorPresence  present;

  float    windSpeed   = 0.0f;   // m/s
  uint16_t windLevel   = 0;      // Beaufort 0-17

  uint32_t lastUpdateMs = 0;
  bool     commError    = false;
  String   errorMsg     = "";
};

// ─── Konfigurace snimace (uklada se do NVS) ───────────────────────────────────
struct SensorConfig {
  uint8_t  modbusAddr    = DEFAULT_MODBUS_ADDR;
  uint32_t baudRate      = DEFAULT_BAUD_RATE;
  uint8_t  pollInterval  = DEFAULT_POLL_INTERVAL;
};

// ─── NVS load / save ──────────────────────────────────────────────────────────
inline void loadConfig(Preferences &p, SensorConfig &c) {
  c.modbusAddr      = p.getUChar("mbAddr",   DEFAULT_MODBUS_ADDR);
  c.baudRate        = p.getUInt ("baud",     DEFAULT_BAUD_RATE);
  c.pollInterval    = p.getUChar("poll",     DEFAULT_POLL_INTERVAL);
}

inline void saveConfig(Preferences &p, const SensorConfig &c) {
  p.putUChar("mbAddr",   c.modbusAddr);
  p.putUInt ("baud",     c.baudRate);
  p.putUChar("poll",     c.pollInterval);
}

// ─── Tabulka Beaufortovy stupnice (cz/en nazev a popis) ──────────────────────
// Podle datasheetu ZTS-3000 (0..17). Hodnoty rozsahu m/s a popisy lze ziskat
// pres funkce nize. Tento snimac ovsem rovnou vraci stupen v reg 0x0001.
struct BeaufortInfo {
  uint8_t     level;
  const char* nameCz;   // cesky nazev
  const char* nameEn;   // anglicky nazev
};

inline const BeaufortInfo& beaufortInfo(uint8_t lvl) {
  static const BeaufortInfo TBL[] = {
    { 0, "Bezvetri",   "Calm"            },
    { 1, "Vanek",      "Light air"       },
    { 2, "Slaby vitr", "Light breeze"    },
    { 3, "Mirny vitr", "Gentle breeze"   },
    { 4, "Dosti silny vitr", "Moderate breeze" },
    { 5, "Cerstvy vitr",     "Fresh breeze"    },
    { 6, "Silny vitr",       "Strong breeze"   },
    { 7, "Prudky vitr",      "Near gale"       },
    { 8, "Bourlivy vitr",    "Gale"            },
    { 9, "Vichrice",         "Strong gale"     },
    {10, "Silna vichrice",   "Storm"           },
    {11, "Mohutna vichrice", "Violent storm"   },
    {12, "Orkan",            "Hurricane"       },
    {13, "Orkan",            "Hurricane"       },
    {14, "Orkan",            "Hurricane"       },
    {15, "Orkan",            "Hurricane"       },
    {16, "Orkan",            "Hurricane"       },
    {17, "Orkan",            "Hurricane"       },
  };
  if (lvl > 17) lvl = 17;
  return TBL[lvl];
}

#endif // WS_CFG_H
