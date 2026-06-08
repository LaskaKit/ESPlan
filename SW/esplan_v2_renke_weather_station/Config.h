#pragma once

#include <Arduino.h>
#include <ETH.h>

#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_PHY_ADDR   0
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_POWER  -1
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

static constexpr int PIN_RS485_RX = 36;
static constexpr int PIN_RS485_TX = 4;

static constexpr uint8_t DEFAULT_MODBUS_ADDR = 0x01;
static constexpr uint32_t DEFAULT_MODBUS_BAUD = 4800;

static constexpr uint16_t REG_DATA_START = 500;
static constexpr uint16_t REG_DATA_COUNT = 16;

static constexpr uint16_t REG_CFG_WIND_DIR_OFFSET = 0x6000;
static constexpr uint16_t REG_CFG_RAIN_SENSITIVITY = 0x6003;

// Wind zero command prefix without CRC. CRC is appended automatically in software.
// User-requested raw frame prefix: 01 AB 01 06 01 02 00 5A A9 CD
static constexpr const char* CMD_WIND_ZERO = "01AB01060102005AA9CD";

static constexpr uint16_t REG_CFG_RAIN_ZERO = 0x0104;
static constexpr uint16_t CFG_RAIN_ZERO = 0x005A;

static constexpr unsigned long SENSOR_POLL_MS = 2000;
static constexpr unsigned long DETECT_RECHECK_MS = 30000;
static constexpr unsigned long ETH_LOG_MS = 10000;

static constexpr size_t HTTP_RESERVE_LARGE = 32000;
static constexpr size_t HTTP_RESERVE_MED = 12000;

enum class AirQualityMode : uint8_t {
  Auto = 0,
  PM = 1,
  CO2 = 2
};
