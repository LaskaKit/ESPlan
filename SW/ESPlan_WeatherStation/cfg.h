#ifndef WS_CFG_H
#define WS_CFG_H
/*
 * cfg.h – Konfigurace hardwaru a datové struktury
 *
 * POZOR: Konfigurace HW pinů je pevná (nelze měnit přes web).
 *        Přes webové rozhraní lze měnit pouze komunikační parametry
 *        a kalibrační registry snímače.
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
// DE pin je řízen hardwarově – nepotřebujeme softwarové ovládání

// ─── Modbus výchozí hodnoty ───────────────────────────────────────────────────
#define DEFAULT_MODBUS_ADDR     1
#define DEFAULT_BAUD_RATE       4800
#define DEFAULT_POLL_INTERVAL   5      // sekundy

// ─── Modbus registry snímače (RS-FSXCS-N01-*) ────────────────────────────────
// Čtecí registry (FC03 / FC04), base address 500 (0x01F4)
#define REG_WIND_SPEED      500
#define REG_WIND_FORCE      501
#define REG_WIND_DIR_8      502
#define REG_WIND_DIR_360    503
#define REG_HUMIDITY        504
#define REG_TEMPERATURE     505
#define REG_NOISE           506
#define REG_PM25_CO2        507
#define REG_PM10_CO2        508
#define REG_PRESSURE        509
#define REG_LUX_HIGH        510
#define REG_LUX_LOW         511
#define REG_LUX_100         512
#define REG_RAINFALL        513
#define REG_COMPASS         514
#define REG_SOLAR           515

// Kalibrační / konfigurační registry (FC06)
#define REG_WIND_DIR_OFFSET 0x6000
#define REG_WIND_ZERO       0x6001
#define REG_RAIN_ZERO       0x6002
#define REG_RAIN_SENS       0x6003

// ─── Flags přítomnosti senzorů ────────────────────────────────────────────────
struct SensorPresence {
  bool wind       = false;
  bool tempHum    = false;
  bool pressure   = false;
  bool noise      = false;
  bool pm25       = false;
  bool pm10       = false;
  bool co2        = false;
  bool light      = false;
  bool rainfall   = false;
  bool compass    = false;
  bool solar      = false;
};

// ─── Naměřené hodnoty ─────────────────────────────────────────────────────────
struct SensorData {
  SensorPresence  present;

  float    windSpeed   = 0.0f;
  uint16_t windForce   = 0;
  uint8_t  windDir8    = 0;
  uint16_t windDir360  = 0;

  float    temperature = 0.0f;
  float    humidity    = 0.0f;
  float    pressure    = 0.0f;
  float    noise       = 0.0f;

  uint16_t pm25        = 0;
  uint16_t pm10        = 0;
  uint16_t co2         = 0;

  uint32_t luxFull     = 0;
  uint32_t lux100      = 0;

  float    rainfall    = 0.0f;
  float    compass     = 0.0f;
  uint16_t solar       = 0;

  uint32_t lastUpdateMs = 0;
  bool     commError    = false;
  String   errorMsg     = "";
};

// ─── Konfigurace snímače (ukládá se do NVS) ───────────────────────────────────
struct SensorConfig {
  uint8_t  modbusAddr    = DEFAULT_MODBUS_ADDR;
  uint32_t baudRate      = DEFAULT_BAUD_RATE;
  uint8_t  pollInterval  = DEFAULT_POLL_INTERVAL;

  uint8_t  windDirOffset   = 0;
  uint8_t  rainSensitivity = 0x11;
  uint8_t  slot507Mode     = 0;  // 0=auto, 1=PM, 2=CO2
  uint8_t  slot508Mode     = 0;
};

// ─── NVS load / save ──────────────────────────────────────────────────────────
inline void loadConfig(Preferences &p, SensorConfig &c) {
  c.modbusAddr      = p.getUChar("mbAddr",   DEFAULT_MODBUS_ADDR);
  c.baudRate        = p.getUInt ("baud",     DEFAULT_BAUD_RATE);
  c.pollInterval    = p.getUChar("poll",     DEFAULT_POLL_INTERVAL);
  c.windDirOffset   = p.getUChar("wdOffset", 0);
  c.rainSensitivity = p.getUChar("rainSens", 0x11);
  c.slot507Mode     = p.getUChar("slot507",  0);
  c.slot508Mode     = p.getUChar("slot508",  0);
}

inline void saveConfig(Preferences &p, const SensorConfig &c) {
  p.putUChar("mbAddr",   c.modbusAddr);
  p.putUInt ("baud",     c.baudRate);
  p.putUChar("poll",     c.pollInterval);
  p.putUChar("wdOffset", c.windDirOffset);
  p.putUChar("rainSens", c.rainSensitivity);
  p.putUChar("slot507",  c.slot507Mode);
  p.putUChar("slot508",  c.slot508Mode);
}

inline const char* windDir8Name(uint8_t d) {
  static const char* names[] = {"N","NE","E","SE","S","SW","W","NW"};
  return (d < 8) ? names[d] : "?";
}

#endif // WS_CFG_H
