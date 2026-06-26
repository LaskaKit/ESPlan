/*
 * snr.cpp – Detekce a čtení snímače RS-FSXCS-N01-*
 *
 * STRATEGIE DETEKCE:
 * Snímač RS-FSXCS-N01-* existuje v různých variantách. Neosazené
 * senzory NEVRACÍ Modbus exception – vrátí 0x0000. Proto:
 *
 * 1. Registry čteme PO SKUPINÁCH
 * 2. Čteme DVAKRÁT s 2s pauzou
 * 3. Podmínky přítomnosti:
 *    - Teplota/vlhkost: přítomno pokud alespoň jedna hodnota != 0
 *      (obě přesně 0.0 je fyzicky extrémně nepravděpodobné)
 *    - Tlak: přítomno pokud > 0 (0 kPa je na Zemi nemožné)
 *    - Hluk: přítomno pokud >= 300 (minimum 30 dB, reg ×10)
 *    - Vítr: přítomno pokud se alespoň jeden reg změní, nebo
 *      je nenulový aspoň v jednom čtení
 *    - PM/CO2: přítomno pokud alespoň jedno čtení != 0
 *    - Osvit: přítomno pokud alespoň jedno čtení != 0
 *    - Srážky: vždy 0 pokud neprší – ověřujeme zápisem do
 *      kalibračního reg 0x6002 (pokud exception → neosazeno)
 *    - Kompas: přítomno pokud se hodnota mění, nebo je nenulová
 *    - Solar: přítomno pokud alespoň jedno čtení != 0
 */

#include "snr.h"

static float rawToTemp(uint16_t raw) {
  return (float)(int16_t)raw / 10.0f;
}

// ─── Detekce ──────────────────────────────────────────────────────────────────
void detectSensors(ModbusClient& mb, SensorData& data, const SensorConfig& cfg) {
  SensorPresence& p = data.present;
  uint8_t addr = cfg.modbusAddr;

  // Resetujeme vše na nepřítomno
  p = SensorPresence();

  Serial.println(F("[SNS] === Detekce senzoru (2x cteni) ==="));

  // Čtení #1: celý blok 500–515
  uint16_t r1[16] = {0};
  ModbusStatus st1 = mb.readRegisters(addr, REG_WIND_SPEED, 16, r1);
  if (st1 != ModbusStatus::OK) {
    Serial.printf("[SNS] Chyba 1. cteni: %s\n", mb.statusStr(st1));
    data.commError = true;
    data.errorMsg  = mb.statusStr(st1);
    return;
  }

  Serial.print(F("[SNS] Cteni #1: "));
  for (int i = 0; i < 16; i++) { Serial.printf("%04X ", r1[i]); }
  Serial.println();

  // Pauza 2 s – dáme senzorům čas vrátit různé hodnoty
  delay(2000);

  // Čtení #2
  uint16_t r2[16] = {0};
  ModbusStatus st2 = mb.readRegisters(addr, REG_WIND_SPEED, 16, r2);
  if (st2 != ModbusStatus::OK) {
    Serial.printf("[SNS] Chyba 2. cteni: %s\n", mb.statusStr(st2));
    data.commError = true;
    data.errorMsg  = mb.statusStr(st2);
    return;
  }

  Serial.print(F("[SNS] Cteni #2: "));
  for (int i = 0; i < 16; i++) { Serial.printf("%04X ", r2[i]); }
  Serial.println();

  data.commError = false;
  data.errorMsg  = "";

  // ── VÍTR (reg 500–503): windSpeed, windForce, windDir8, windDir360 ───
  // Vítr (ultrazvukový) je ZÁKLADNÍ senzor, přítomný na VŠECH variantách
  // RS-FSXCS-N01-*. Při bezvětří vrací 0,0,0,0 což je validní stav.
  // Proto: pokud čtení bloku proběhlo OK, vítr je vždy přítomný.
  {
    p.wind = true;
    Serial.printf("[SNS]  Vitr:  r1=[%d,%d,%d,%d] r2=[%d,%d,%d,%d] => ANO (zakladni senzor)\n",
                  r1[0], r1[1], r1[2], r1[3],
                  r2[0], r2[1], r2[2], r2[3]);
  }

  // ── TEPLOTA + VLHKOST (reg 504–505) ──────────────────────────────────
  // 504=vlhkost×10, 505=teplota×10 (signed)
  // Obě přesně 0 je fyzicky téměř nemožné (0.0°C a 0.0%RH)
  {
    bool ok1 = (r1[4] != 0 || r1[5] != 0);
    bool ok2 = (r2[4] != 0 || r2[5] != 0);
    p.tempHum = ok1 || ok2;
    Serial.printf("[SNS]  Temp/RH: r1=[%d,%d] r2=[%d,%d] => %s\n",
                  r1[4], r1[5], r2[4], r2[5], p.tempHum?"ANO":"NE");
  }

  // ── HLUK (reg 506) ───────────────────────────────────────────────────
  // Rozsah snímače 30–120 dB → registr 300–1200 (×10).
  // POZOR: neosazený senzor vrací konstantně 300 (= 30.0 dB).
  // Přítomno pokud: hodnota > 300, NEBO se mění mezi čteními.
  // Přesně 300 v obou čteních = pravděpodobně neosazeno.
  {
    bool inRange1  = (r1[6] >= 300 && r1[6] <= 1200);
    bool inRange2  = (r2[6] >= 300 && r2[6] <= 1200);
    bool changed   = (r1[6] != r2[6]);
    bool aboveMin  = (r1[6] > 300 || r2[6] > 300);

    // Přítomno pokud je v rozsahu A (hodnota nad minimem NEBO se mění)
    p.noise = (inRange1 || inRange2) && (aboveMin || changed);
    Serial.printf("[SNS]  Hluk:  r1=%d r2=%d chg=%d abv=%d => %s\n",
                  r1[6], r2[6], changed, aboveMin, p.noise?"ANO":"NE");
  }

  // ── PM2.5/PM10 nebo CO2 (reg 507–508) ───────────────────────────────
  // Přítomno pokud alespoň v jednom čtení != 0
  {
    bool anyNZ1 = (r1[7] != 0 || r1[8] != 0);
    bool anyNZ2 = (r2[7] != 0 || r2[8] != 0);
    bool present = anyNZ1 || anyNZ2;

    if (present) {
      uint8_t mode = cfg.slot507Mode;
      if (mode == 0) {
        // Auto: CO2 typicky 400–5000 ppm
        uint16_t v = (r2[7] != 0) ? r2[7] : r1[7];
        mode = (v >= 400 && v <= 5000) ? 2 : 1;
      }
      if (mode == 2) {
        p.co2  = true;
      } else {
        p.pm25 = true;
        p.pm10 = true;
      }
    }
    Serial.printf("[SNS]  PM/CO2: r1=[%d,%d] r2=[%d,%d] => pm25=%s pm10=%s co2=%s\n",
                  r1[7], r1[8], r2[7], r2[8],
                  p.pm25?"ANO":"NE", p.pm10?"ANO":"NE", p.co2?"ANO":"NE");
  }

  // ── TLAK (reg 509) ──────────────────────────────────────────────────
  // 0 kPa je na Zemi fyzicky nemožné. Normální rozsah 85–108 kPa.
  {
    p.pressure = (r1[9] > 0 || r2[9] > 0);
    Serial.printf("[SNS]  Tlak:  r1=%d r2=%d => %s\n",
                  r1[9], r2[9], p.pressure?"ANO":"NE");
  }

  // ── OSVIT (reg 510–512): luxH, luxL, lux/100 ───────────────────────
  // 0 lux = noc/tma, validní. Ale pokud ALL 0 v obou čteních = neosazeno.
  {
    bool allZ1 = (r1[10]==0 && r1[11]==0 && r1[12]==0);
    bool allZ2 = (r2[10]==0 && r2[11]==0 && r2[12]==0);
    p.light = !(allZ1 && allZ2);
    Serial.printf("[SNS]  Osvit: r1=[%d,%d,%d] r2=[%d,%d,%d] => %s\n",
                  r1[10],r1[11],r1[12], r2[10],r2[11],r2[12],
                  p.light?"ANO":"NE");
  }

  // ── SRÁŽKY (reg 513) ────────────────────────────────────────────────
  // 0 mm je validní (neprší). Hodnota je kumulativní.
  // Ověříme zápisem do kalibračního reg: pokud snímač nemá
  // srážkoměr, zápis do REG_RAIN_ZERO (0x6002) vrátí exception.
  {
    if (r1[13] > 0 || r2[13] > 0) {
      p.rainfall = true;
    } else {
      // Hodnota je 0 – pokusíme se ověřit přes kalibrační registr
      // Čteme reg 0x6003 (rain sensitivity) – pokud OK, srážkoměr je
      uint16_t dummy = 0;
      ModbusStatus st = mb.readRegisters(addr, REG_RAIN_SENS, 1, &dummy);
      p.rainfall = (st == ModbusStatus::OK && dummy != 0 && dummy != 0xFFFF);
      Serial.printf("[SNS]  Srazky: kalib.reg test: %s (val=0x%04X)\n",
                    mb.statusStr(st), dummy);
    }
    Serial.printf("[SNS]  Srazky: r1=%d r2=%d => %s\n",
                  r1[13], r2[13], p.rainfall?"ANO":"NE");
  }

  // ── KOMPAS (reg 514) ────────────────────────────────────────────────
  // 0 = sever, validní. Ale neosazený kompas vrací stále 0.
  // Přítomno pokud nenulový, nebo se mění.
  {
    bool nonZero = (r1[14] != 0 || r2[14] != 0);
    bool changed = (r1[14] != r2[14]);
    p.compass = nonZero || changed;
    Serial.printf("[SNS]  Kompas: r1=%d r2=%d => %s\n",
                  r1[14], r2[14], p.compass?"ANO":"NE");
  }

  // ── SOLÁRNÍ ZÁŘENÍ (reg 515) ────────────────────────────────────────
  // 0 W/m² validní (noc). Přítomno pokud alespoň jednou != 0.
  {
    p.solar = (r1[15] != 0 || r2[15] != 0);
    Serial.printf("[SNS]  Solar: r1=%d r2=%d => %s\n",
                  r1[15], r2[15], p.solar?"ANO":"NE");
  }

  Serial.println(F("[SNS] === Detekce dokoncena ==="));
}

// ─── Čtení všech snímačů (bulk) ─────────────────────────────────────────────
void readAllSensors(ModbusClient& mb, SensorData& data, const SensorConfig& cfg) {
  uint16_t buf[16];
  ModbusStatus st = mb.readRegisters(cfg.modbusAddr, REG_WIND_SPEED, 16, buf);

  if (st != ModbusStatus::OK) {
    data.commError = true;
    data.errorMsg  = mb.statusStr(st);
    Serial.printf("[SNS] Chyba cteni: %s\n", mb.statusStr(st));
    return;
  }

  data.commError    = false;
  data.errorMsg     = "";
  data.lastUpdateMs = millis();

  const SensorPresence& p = data.present;

  if (p.wind) {
    data.windSpeed  = buf[0] / 10.0f;
    data.windForce  = buf[1];
    data.windDir8   = (uint8_t)(buf[2] & 0x07);
    data.windDir360 = buf[3];
  }

  if (p.tempHum) {
    data.humidity    = buf[4] / 10.0f;
    data.temperature = rawToTemp(buf[5]);
  }

  if (p.noise)    data.noise    = buf[6] / 10.0f;
  if (p.pressure) data.pressure = buf[9] / 10.0f;

  // PM / CO2
  if (p.co2)  data.co2  = buf[7];
  if (p.pm25) data.pm25 = buf[7];
  if (p.pm10) data.pm10 = buf[8];

  if (p.light) {
    data.luxFull = ((uint32_t)buf[10] << 16) | buf[11];
    data.lux100  = buf[12];
  }
  if (p.rainfall) data.rainfall = buf[13] / 10.0f;
  if (p.compass)  data.compass  = buf[14] / 100.0f;
  if (p.solar)    data.solar    = buf[15];
}

// ─── Výpis stavu ─────────────────────────────────────────────────────────────
void printSensorStatus(const SensorData& data) {
  const SensorPresence& p = data.present;
  Serial.println(F("[SNS] +--------------------------+"));
  Serial.println(F("[SNS] | Detekce senzoru          |"));
  Serial.println(F("[SNS] +--------------------------+"));
  Serial.printf("[SNS] | Vitr:       %-3s          |\n", p.wind     ?"ANO":"NE");
  Serial.printf("[SNS] | Teplota/RH: %-3s          |\n", p.tempHum  ?"ANO":"NE");
  Serial.printf("[SNS] | Tlak:       %-3s          |\n", p.pressure ?"ANO":"NE");
  Serial.printf("[SNS] | Hluk:       %-3s          |\n", p.noise    ?"ANO":"NE");
  Serial.printf("[SNS] | PM2.5:      %-3s          |\n", p.pm25     ?"ANO":"NE");
  Serial.printf("[SNS] | PM10:       %-3s          |\n", p.pm10     ?"ANO":"NE");
  Serial.printf("[SNS] | CO2:        %-3s          |\n", p.co2      ?"ANO":"NE");
  Serial.printf("[SNS] | Osvit:      %-3s          |\n", p.light    ?"ANO":"NE");
  Serial.printf("[SNS] | Srazky:     %-3s          |\n", p.rainfall ?"ANO":"NE");
  Serial.printf("[SNS] | Kompas:     %-3s          |\n", p.compass  ?"ANO":"NE");
  Serial.printf("[SNS] | Solar:      %-3s          |\n", p.solar    ?"ANO":"NE");
  Serial.println(F("[SNS] +--------------------------+"));
}