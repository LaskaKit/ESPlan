/*
 * ESPlan_ZTS3000_WindSensor.ino
 * ZTS-3000-FSJT-N01 - Polykarbonatovy 3-poharovy anemometr
 *
 * Hardware  : LaskaKit ESPlan (ESP32 + LAN8720)
 * Snimac    : ZTS-3000-FSJT-N01 (Modbus RTU / RS485)
 * Komunikace: pouze ETH (LAN8720)
 *
 * Soubory projektu:
 *   cfg.h           - HW konstanty, registry, datove struktury
 *   mb_client.h/cpp - Modbus RTU klient (FC03, FC06)
 *   snr.h/cpp       - Detekce a cteni snimace
 *   i18n_str.h      - Preklady CZ / EN
 *   ws_server.h/cpp - HTTP webserver (stav + konfigurace)
 *
 * Modbus-RTU parametry snimace (datasheet):
 *   adresa: 0x01 (vychozi), bps: 4800, 8N1
 *   FC03 cteni 2 registru od 0x0000:
 *     0x0000 = rychlost vetru x10 (m/s)
 *     0x0001 = sila vetru (Beaufort 0-17)
 *   FC06 zapis adresy do reg 0x07D0
 */

#include <Arduino.h>
#include <WiFi.h>          // nutne pro WiFiEvent_t a WiFi.onEvent
#include <ETH.h>
#include <Preferences.h>

#include "cfg.h"
#include "mb_client.h"
#include "snr.h"
#include "ws_server.h"

// ─── Globalni objekty ─────────────────────────────────────────────────────────
Preferences   prefs;
SensorData    sensorData;
SensorConfig  sensorConfig;
ModbusClient  modbusClient;

static bool     ethGotIP   = false;
static uint32_t lastPollMs = 0;

// ─── ETH udalosti ─────────────────────────────────────────────────────────────
void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println(F("[ETH] Inicializace..."));
      ETH.setHostname("espplan-zts3000");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println(F("[ETH] Pripojen (fyzicka vrstva)"));
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print(F("[ETH] IP: "));
      Serial.println(ETH.localIP());
      ethGotIP = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println(F("[ETH] Odpojeno"));
      ethGotIP = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println(F("[ETH] Zastaveno"));
      break;
    default:
      break;
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== ESPlan ZTS-3000 Anemometr ==="));
  Serial.println(F("    ZTS-3000-FSJT-N01 Modbus RTU "));
  Serial.println(F("================================"));

  // Nacteni konfigurace z NVS
  prefs.begin("zts3000", true);
  loadConfig(prefs, sensorConfig);
  prefs.end();

  Serial.printf("[CFG] Modbus addr=%d baud=%d poll=%ds\n",
                sensorConfig.modbusAddr,
                sensorConfig.baudRate,
                sensorConfig.pollInterval);

  // RS485 / Modbus
  modbusClient.begin(sensorConfig.baudRate, sensorConfig.modbusAddr);

  // ETH
  WiFi.onEvent(onEthEvent);
  ETH.begin(ETH_PHY_TYPE,
            ETH_PHY_ADDR,
            ETH_PHY_MDC,
            ETH_PHY_MDIO,
            ETH_PHY_POWER,
            ETH_CLK_MODE);

  // Cekame na IP (max 10 s)
  uint32_t t0 = millis();
  while (!ethGotIP && (millis() - t0 < 10000)) {
    delay(100);
  }

  if (!ethGotIP) {
    Serial.println(F("[ETH] VAROVANI: IP neziskano - web nebude dostupny!"));
  }

  // Webserver
  webServerBegin(sensorData, sensorConfig, prefs, modbusClient);

  // Prvni detekce
  Serial.println(F("[SNS] Detekuji snimac..."));
  detectSensors(modbusClient, sensorData, sensorConfig);
  printSensorStatus(sensorData);

  Serial.println(F("[SYS] Inicializace dokoncena."));
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  webServerHandle();

  uint32_t now = millis();
  if (now - lastPollMs >= (uint32_t)sensorConfig.pollInterval * 1000UL) {
    lastPollMs = now;
    readAllSensors(modbusClient, sensorData, sensorConfig);
  }

  delay(5);
}
