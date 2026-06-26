/*
 * snr.cpp – Detekce a cteni snimace ZTS-3000-FSJT-N01
 *
 * Snimac ma 2 registry pristupne pres FC03:
 *   reg 0x0000 = rychlost vetru x10 (m/s)
 *   reg 0x0001 = sila vetru (Beaufort 0-17)
 *
 * Detekce: pokud cteni 2 registru projde (CRC OK, zadna exception),
 * snimac povazujeme za pritomny. Bezvetri (0,0) je validni stav.
 */

#include "snr.h"

// ─── Detekce ──────────────────────────────────────────────────────────────────
void detectSensors(ModbusClient& mb, SensorData& data, const SensorConfig& cfg) {
  SensorPresence& p = data.present;
  uint8_t addr = cfg.modbusAddr;

  p = SensorPresence();   // reset

  Serial.println(F("[SNS] === Detekce ZTS-3000-FSJT-N01 ==="));

  uint16_t r[REG_BLOCK_COUNT] = {0};
  ModbusStatus st = mb.readRegisters(addr, REG_WIND_SPEED, REG_BLOCK_COUNT, r);

  if (st != ModbusStatus::OK) {
    Serial.printf("[SNS] Chyba cteni: %s\n", mb.statusStr(st));
    data.commError = true;
    data.errorMsg  = mb.statusStr(st);
    return;
  }

  Serial.printf("[SNS] Cteni: speed_raw=%u (=%.1f m/s)  level=%u\n",
                r[0], r[0] / 10.0f, r[1]);

  data.commError = false;
  data.errorMsg  = "";
  p.wind         = true;

  // rovnou ulozime hodnoty
  data.windSpeed    = r[0] / 10.0f;
  data.windLevel    = r[1];
  data.lastUpdateMs = millis();

  Serial.println(F("[SNS] === Detekce dokoncena: snimac pritomny ==="));
}

// ─── Cteni snimace ────────────────────────────────────────────────────────────
void readAllSensors(ModbusClient& mb, SensorData& data, const SensorConfig& cfg) {
  uint16_t r[REG_BLOCK_COUNT] = {0};
  ModbusStatus st = mb.readRegisters(cfg.modbusAddr, REG_WIND_SPEED,
                                     REG_BLOCK_COUNT, r);

  if (st != ModbusStatus::OK) {
    data.commError = true;
    data.errorMsg  = mb.statusStr(st);
    Serial.printf("[SNS] Chyba cteni: %s\n", mb.statusStr(st));
    return;
  }

  data.commError    = false;
  data.errorMsg     = "";
  data.lastUpdateMs = millis();

  // Pokud predtim selhala detekce, oznacime snimac jako pritomny
  if (!data.present.wind) data.present.wind = true;

  data.windSpeed = r[0] / 10.0f;
  data.windLevel = r[1];
}

// ─── Vypis stavu ─────────────────────────────────────────────────────────────
void printSensorStatus(const SensorData& data) {
  const SensorPresence& p = data.present;
  Serial.println(F("[SNS] +--------------------------+"));
  Serial.println(F("[SNS] | ZTS-3000-FSJT-N01        |"));
  Serial.println(F("[SNS] +--------------------------+"));
  Serial.printf ("[SNS] | Snimac pritomny: %-3s     |\n", p.wind ? "ANO" : "NE");
  if (p.wind) {
    const BeaufortInfo& bi = beaufortInfo(data.windLevel);
    Serial.printf("[SNS] | Rychlost: %5.1f m/s      |\n", data.windSpeed);
    Serial.printf("[SNS] | Sila:     %u (%s) |\n", data.windLevel, bi.nameCz);
  }
  Serial.println(F("[SNS] +--------------------------+"));
}
