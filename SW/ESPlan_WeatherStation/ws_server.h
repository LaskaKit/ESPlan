#ifndef WS_SERVER_H
#define WS_SERVER_H
/*
 * ws_server.h – HTTP webserver
 *
 * Stránky:
 *   GET  /            Stav snímačů + živá data
 *   GET  /config      Konfigurace snímače
 *   POST /config      Uložení konfigurace
 *   POST /detect      Re-detekce snímačů
 *   POST /action      Akce: wind_zero, rain_zero
 *   GET  /api/data    JSON s aktuálními daty
 *   GET  /api/status  JSON se stavem detekce
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
