#ifndef RL_WS_SERVER_H
#define RL_WS_SERVER_H
/*
 * ws_server.h – HTTP webserver pro rele modul
 *
 * Stranky:
 *   GET  /            Ovladani rele (8 kanalu + hromadne)
 *   GET  /config      Konfigurace
 *   POST /config      Ulozeni konfigurace
 *   POST /relay       Akce: on/off/toggle/all_on/all_off
 *   POST /action      Zmena adresy modulu
 *   GET  /api/state   JSON se stavem
 *   GET  /api/raw     JSON s poslednim RS485 TX/RX
 */

#include <WebServer.h>
#include <Preferences.h>
#include "cfg.h"
#include "mb_client.h"
#include "relay.h"
#include "i18n_str.h"

void webServerBegin(RelayState& state, RelayConfig& cfg,
                    Preferences& prefs, ModbusClient& mb);

void webServerHandle();

#endif // RL_WS_SERVER_H
