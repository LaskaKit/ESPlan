/*
 * relay.cpp – Ovladani RS485 rele modulu (8 / 16 kanalu)
 */

#include "relay.h"

static bool sendCmd(ModbusClient& mb, RelayState& st, uint8_t addr,
                    uint16_t reg, uint16_t val, const String& actionDesc) {
  ModbusStatus s = mb.writeRegister(addr, reg, val);
  st.lastCmdMs  = millis();
  st.lastAction = actionDesc;
  if (s == ModbusStatus::OK) {
    st.commError = false;
    st.errorMsg  = "";
    Serial.printf("[REL] %s : OK\n", actionDesc.c_str());
    return true;
  } else {
    st.commError = true;
    st.errorMsg  = mb.statusStr(s);
    Serial.printf("[REL] %s : %s\n", actionDesc.c_str(), mb.statusStr(s));
    return false;
  }
}

static inline bool validCh(uint8_t channel, const RelayConfig& cfg) {
  return (channel >= 1 && channel <= cfg.channelCount);
}

bool relaySet(ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
              uint8_t channel, bool on) {
  if (!validCh(channel, cfg)) return false;
  uint16_t reg = REG_CHANNEL_BASE + (channel - 1);
  uint16_t val = makeVal(on ? FN_OPEN : FN_CLOSE);
  String   d   = String("ch") + channel + (on ? " ON" : " OFF");
  bool ok = sendCmd(mb, st, cfg.modbusAddr, reg, val, d);
  if (ok) st.channels[channel - 1] = on;
  return ok;
}

bool relayToggle(ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                 uint8_t channel) {
  if (!validCh(channel, cfg)) return false;
  uint16_t reg = REG_CHANNEL_BASE + (channel - 1);
  uint16_t val = makeVal(FN_TOGGLE);
  String   d   = String("ch") + channel + " TOGGLE";
  bool ok = sendCmd(mb, st, cfg.modbusAddr, reg, val, d);
  if (ok) st.channels[channel - 1] = !st.channels[channel - 1];
  return ok;
}

bool relayLatch(ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                uint8_t channel) {
  if (!validCh(channel, cfg)) return false;
  uint16_t reg = REG_CHANNEL_BASE + (channel - 1);
  uint16_t val = makeVal(FN_LATCH);
  String   d   = String("ch") + channel + " LATCH";
  bool ok = sendCmd(mb, st, cfg.modbusAddr, reg, val, d);
  if (ok) {
    // Inter-locking: jen tento kanal ON, ostatni OFF
    for (uint8_t i = 0; i < cfg.channelCount; i++) {
      st.channels[i] = (i == (channel - 1));
    }
  }
  return ok;
}

bool relayMomentary(ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                    uint8_t channel) {
  if (!validCh(channel, cfg)) return false;
  uint16_t reg = REG_CHANNEL_BASE + (channel - 1);
  uint16_t val = makeVal(FN_MOMENTARY);
  String   d   = String("ch") + channel + " MOMENTARY (1s)";
  // Pri momentary se rele po 1s vrati do OFF. SW shadow nemenime –
  // dalsi periodicky FC03 sync stav doplni.
  return sendCmd(mb, st, cfg.modbusAddr, reg, val, d);
}

bool relayDelay(ModbusClient& mb, RelayState& st, const RelayConfig& cfg,
                uint8_t channel, uint8_t seconds) {
  if (!validCh(channel, cfg)) return false;
  uint16_t reg = REG_CHANNEL_BASE + (channel - 1);
  uint16_t val = makeVal(FN_DELAY, seconds);
  String   d   = String("ch") + channel + " DELAY " + seconds + "s";
  bool ok = sendCmd(mb, st, cfg.modbusAddr, reg, val, d);
  if (ok) st.channels[channel - 1] = true;   // okamzite ON, pak FC03 zjisti OFF
  return ok;
}

bool relayAll(ModbusClient& mb, RelayState& st, const RelayConfig& cfg, bool on) {
  uint16_t val = makeVal(on ? FN_ALL_ON : FN_ALL_OFF);
  String   d   = on ? "ALL ON" : "ALL OFF";
  bool ok = sendCmd(mb, st, cfg.modbusAddr, REG_ALL_CHANNELS, val, d);
  if (ok) {
    for (uint8_t i = 0; i < cfg.channelCount; i++) st.channels[i] = on;
  }
  return ok;
}

bool relayReadAll(ModbusClient& mb, RelayState& st, const RelayConfig& cfg) {
  uint16_t r[MAX_CHANNEL_COUNT] = {0};
  ModbusStatus s = mb.readRegisters(cfg.modbusAddr,
                                    REG_CHANNEL_BASE, cfg.channelCount, r);
  st.lastUpdateMs = millis();
  if (s != ModbusStatus::OK) {
    Serial.printf("[REL] Cteni stavu: %s (shadow zachovan)\n",
                  mb.statusStr(s));
    return false;
  }
  // Dle datasheetu: 0x0001 = open(ON), 0x0000 = close(OFF)
  for (uint8_t i = 0; i < cfg.channelCount; i++) {
    st.channels[i] = (r[i] != 0);
  }
  return true;
}
