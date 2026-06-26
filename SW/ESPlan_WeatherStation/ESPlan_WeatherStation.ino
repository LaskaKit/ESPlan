/*
 * ESPlan_WeatherStation.ino
 * RS-FSXCS-N01-* Ultrazvuková meteorologická stanice
 *
 * Hardware : LaskaKit ESPlan (ESP32 + LAN8720)
 * Senzor   : RS-FSXCS-N01-* (Modbus RTU / RS485)
 * Komunikace: pouze ETH (LAN8720)
 *
 * Soubory projektu:
 *   cfg.h          – HW konstanty, registry, datové struktury
 *   mb_client.h/cpp– Modbus RTU klient (FC03, FC06)
 *   snr.h/cpp      – Detekce variant a čtení snímače
 *   i18n_str.h     – Překlady CZ / EN
 *   ws_server.h/cpp– HTTP webserver (stav + konfigurace)
 */

#include <Arduino.h>
#include <WiFi.h>          // nutné pro WiFiEvent_t a WiFi.onEvent
#include <ETH.h>
#include <Preferences.h>

#include "cfg.h"
#include "mb_client.h"
#include "snr.h"
#include "ws_server.h"

// ─── Globální objekty ─────────────────────────────────────────────────────────
Preferences   prefs;
SensorData    sensorData;
SensorConfig  sensorConfig;
ModbusClient  modbusClient;

static bool     ethGotIP   = false;
static uint32_t lastPollMs = 0;

// ─── ETH události ─────────────────────────────────────────────────────────────
void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println(F("[ETH] Inicializace..."));
      ETH.setHostname("espplan-weather");
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
  Serial.println(F("\n=== ESPlan Weather Station ==="));
  Serial.println(F("    RS-FSXCS-N01-* Modbus RTU "));
  Serial.println(F("=============================="));

  // Načtení konfigurace z NVS
  prefs.begin("weather", true);
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

  // Čekáme na IP (max 10 s)
  uint32_t t0 = millis();
  while (!ethGotIP && (millis() - t0 < 10000)) {
    delay(100);
  }

  if (!ethGotIP) {
    Serial.println(F("[ETH] VAROVANI: IP neziskano – web nebude dostupny!"));
  }

  // Webserver
  webServerBegin(sensorData, sensorConfig, prefs, modbusClient);

  // První detekce
  Serial.println(F("[SNS] Detekuji senzory..."));
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
