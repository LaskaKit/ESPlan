/*
 * ESPlan_RS485_Relay.ino
 * 8 / 16-kanalovy RS485 rele modul (Modbus-RTU)
 *
 * Hardware : LaskaKit ESPlan (ESP32 + LAN8720)
 * Modul    : 8-ch / 16-ch 12V rele 250VAC/10A RS485 DIN
 *            https://www.laskakit.cz/8-kanalu-12v-rele-modul-250vac-10a--rs485--din/
 *
 * Soubory projektu:
 *   cfg.h           - HW konstanty, registry, datove struktury
 *   mb_client.h/cpp - Modbus RTU klient (FC03, FC06)
 *   relay.h/cpp     - Ovladani rele (set/toggle/latch/momentary/delay/all/read)
 *   i18n_str.h      - Preklady CZ / EN
 *   ws_server.h/cpp - HTTP webserver
 *
 * Modbus protokol (datasheet 8ch_rele_rs485_protocols.pdf):
 *   FC 0x06 (Write Single Register), 8N1
 *   Adresa modulu se nastavuje DIP prepinaci A0-A5 (rozsah 0-47)
 *   Baudrate (HW pady M1/M2): 9600 (default) / 2400 / 4800 / 19200
 *
 *   reg 0x0001..0x0010  - rele 1..16 (jednotlive kanaly)
 *     val 0x0100 = Open (sepne)
 *     val 0x0200 = Close (rozepne)
 *     val 0x0300 = Toggle (self-locking)
 *     val 0x0400 = Latch (inter-locking)
 *     val 0x0500 = Momentary (1s puls)
 *     val 0x06xx = Delay xx s (0-255)
 *   reg 0x0000  - vsechny kanaly
 *     val 0x0700 = Vsechny ON
 *     val 0x0800 = Vsechny OFF
 *
 *   FC 0x03 cteni stavu: reg 0x0001..0x0010, vrati 0x0001=ON / 0x0000=OFF
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <Preferences.h>

#include "cfg.h"
#include "mb_client.h"
#include "relay.h"
#include "ws_server.h"

// ─── Globalni objekty ─────────────────────────────────────────────────────────
Preferences   prefs;
RelayState    relayState;
RelayConfig   relayCfg;
ModbusClient  modbusClient;

static bool     ethGotIP   = false;
static uint32_t lastReadMs = 0;
static const uint32_t READ_INTERVAL_MS = 5000;  // 5s pravidelna sync z modulu

// ─── ETH udalosti ─────────────────────────────────────────────────────────────
void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println(F("[ETH] Inicializace..."));
      ETH.setHostname("espplan-relay");
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
  Serial.println(F("\n=== ESPlan RS485 Relay 8/16ch ==="));
  Serial.println(F("    LaskaKit 8/16-ch Modbus-RTU "));
  Serial.println(F("============================="));

  // Nacteni konfigurace z NVS
  prefs.begin("rs485rel", true);
  loadConfig(prefs, relayCfg);
  prefs.end();

  Serial.printf("[CFG] Modbus addr=%d baud=%d\n",
                relayCfg.modbusAddr,
                relayCfg.baudRate);

  // RS485 / Modbus
  modbusClient.begin(relayCfg.baudRate, relayCfg.modbusAddr);

  // ETH
  WiFi.onEvent(onEthEvent);
  ETH.begin(ETH_PHY_TYPE,
            ETH_PHY_ADDR,
            ETH_PHY_MDC,
            ETH_PHY_MDIO,
            ETH_PHY_POWER,
            ETH_CLK_MODE);

  uint32_t t0 = millis();
  while (!ethGotIP && (millis() - t0 < 10000)) {
    delay(100);
  }
  if (!ethGotIP) {
    Serial.println(F("[ETH] VAROVANI: IP neziskano - web nebude dostupny!"));
  }

  // Webserver
  webServerBegin(relayState, relayCfg, prefs, modbusClient);

  // Pokus o nacteni stavu (pokud modul podporuje FC03)
  Serial.println(F("[REL] Pokus o sync stavu z modulu..."));
  relayReadAll(modbusClient, relayState, relayCfg);

  Serial.println(F("[SYS] Inicializace dokoncena."));
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  webServerHandle();

  // Periodicky pokus o aktualizaci stavu (volitelne)
  uint32_t now = millis();
  if (now - lastReadMs >= READ_INTERVAL_MS) {
    lastReadMs = now;
    relayReadAll(modbusClient, relayState, relayCfg);
  }

  delay(5);
}
