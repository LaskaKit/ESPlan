#ifndef WS_SERVER_H
#define WS_SERVER_H
/*
 * ws_server.h – HTTP webserver
 *
 * Stranky:
 *   GET  /            Stav snimace + ziva data (wind speed + Beaufort)
 *   GET  /config      Konfigurace snimace
 *   POST /config      Ulozeni konfigurace
 *   POST /detect      Re-detekce snimace
 *   POST /action      Akce: change_addr
 *   GET  /api/data    JSON s aktualnimi daty
 *   GET  /api/status  JSON se stavem detekce
 *   GET  /api/raw     Posledni RS485 TX/RX
 */

#include <WebServer.h>
#include <Preferences.h>
#include "cfg.h"
#include "mb_client.h"
#include "snr.h"
#include "i18n_str.h"

void webServerBegin(SensorData& data, SensorConfig& cfg,
                    Preferences& prefs, ModbusClient& mb);

void webServerHandle();

#endif // WS_SERVER_H
