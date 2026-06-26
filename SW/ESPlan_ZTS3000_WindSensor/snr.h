#ifndef WS_SNR_H
#define WS_SNR_H
/*
 * snr.h – Detekce a cteni snimace ZTS-3000-FSJT-N01
 */

#include "cfg.h"
#include "mb_client.h"

void detectSensors(ModbusClient& mb, SensorData& data, const SensorConfig& cfg);
void readAllSensors(ModbusClient& mb, SensorData& data, const SensorConfig& cfg);
void printSensorStatus(const SensorData& data);

#endif // WS_SNR_H
