#ifndef RL_RELAY_H
#define RL_RELAY_H
/*
 * relay.h – Ovladani RS485 rele modulu (8 / 16 kanalu)
 *
 * Vsechny funkce vraci true pri OK odpovedi z modulu. SW stav
 * v RelayState.channels[] se aktualizuje pouze pri OK.
 */

#include "cfg.h"
#include "mb_client.h"

// Open / Close (sepne / rozepne)
bool relaySet      (ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    uint8_t channel /*1..N*/, bool on);

// Toggle (self-locking) – prepne stav
bool relayToggle   (ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    uint8_t channel);

// Latch (inter-locking) – jen tento kanal ON, ostatni OFF
bool relayLatch    (ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    uint8_t channel);

// Momentary (non-locking) – 1s puls
bool relayMomentary(ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    uint8_t channel);

// Delay – sepne na zadany pocet sekund (0-255), pak automaticky rozepne
bool relayDelay    (ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    uint8_t channel, uint8_t seconds);

// Vsechny kanaly ON / OFF
bool relayAll      (ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    bool on);

// Cteni stavu vsech aktivnich kanalu pres FC03
bool relayReadAll  (ModbusClient& mb, RelayState& st, const RelayConfig& cfg);

#endif // RL_RELAY_H
