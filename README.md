# ESPlan v2.1

**Read this in other languages: [Čeština](README_CZ)**

![ESPlan](img/laskakit-esplan-esp32-lan8720a-max485-poe-1.jpg)

**LaskaKit ESPlan** is an industrial-grade ESP32 development board built around the **ESP32-WROOM-32E** module. It combines wired **Ethernet (LAN8720A)**, an **RS485** bus, **Wi-Fi + Bluetooth**, and **three independent ways to power the board** – including optional **PoE**. It is designed for industrial automation, IoT gateways, home automation (Home Assistant, ESPHome) and remote sensor nodes where reliable wired connectivity without dependence on Wi-Fi is required.

Designed and manufactured in the Czech Republic 🇨🇿

Product page: <https://www.laskakit.cz/laskakit-esplan-esp32-lan8720a-max485-poe/>

---

## ⚡ Power – three options

ESPlan v2.1 supports three power sources that can be combined freely. The source with the **highest voltage is always active**.

| Source | Input voltage | Path to the board | Typical use |
| --- | --- | --- | --- |
| **USB-C** | 5 V | direct to the 5 V rail | development, programming |
| **Screw terminal (+12/24 V, GND)** | **7–40 V DC** (12 or 24 V recommended) | step-down **LMR14050** (up to 5 A) → 5 V → LDO **XC6220** → 3.3 V | industrial PSU, DIN rail |
| **PoE (Power over Ethernet)** | via optional **SDaPo DP9900M** module (12 V or 24 V variant, **sold separately**) | PoE module output → same buck converter as the terminal | power straight from the LAN cable |

> **PoE note:** We recommend the **24 V** variant of the DP9900M – it better matches the converter input range and gives higher efficiency. The module meets **IEEE 802.3af** (up to 12.95 W), is galvanically isolated, and works with any PoE switch or injector.

The whole power chain is protected: terminal inputs are fused with a polyfuse, and the RS485 bus has TVS diodes (PSM712) and a polyfuse (MF-MSMF010-2) against overvoltage and shorts from the external line.

See annotated diagrams in [`img/power/`](img/power) for each option.

---

## 🔌 RS485 – wiring & terminal voltage

The RS485 terminal has **four pins: 12/24 V IN/OUT, GND, A, B**.

- **A and B are NOT power pins** – they carry the differential *data* signal from the **WS3081** transceiver. At idle the A−B difference is ~0 V; while transmitting, |A−B| is roughly **1.5–3 V** under load (per the RS485 standard the driver outputs ≥1.5 V differential, receivers detect ±200 mV, common-mode range −7 V to +12 V). **There is no usable supply voltage on A/B.**
- **Powering a remote RS485 device:** the terminal block also brings out **12/24 V IN/OUT**, so the same voltage you use to power the ESPlan (12 or 24 V) can be looped out to power a field sensor over the same run. If your sensor needs a different voltage (e.g. 5 V), use a local regulator at the sensor.
- **Bus wiring:** connect **A↔A, B↔B, GND↔GND**. At the end of a long run enable the on-board **120 Ω** termination with the **BUS_TERM** jumper – no external terminator needed.

---

## 🌐 Connectivity & interfaces

- **Ethernet** – LAN8720A PHY (Fast Ethernet 10/100 Mbps), HanRun RJ45 with integrated magnetics and status LEDs.
- **RS485** – WS3081 transceiver with TVS protection, screw terminal (12/24 V IN/OUT, GND, A, B), on-board 120 Ω termination via the `BUS_TERM` jumper.
- **Wi-Fi + Bluetooth** – built into the ESP32-WROOM-32E (2.4 GHz Wi-Fi 802.11 b/g/n, Bluetooth 4.2 / BLE).
- **microSD card** – slot over SPI with card-detect.
- **I²C connector** – dedicated 4-pin connector (3.3V, GND, SCL, SDA) with pull-ups for sensors, displays, etc.
- **SPI connector** – dedicated 6-pin connector (3.3V, GND, MOSI, MISO, SCK, CS).
- **GPIO header** – all free ESP32 pins broken out on a populated header.

---

## 📍 Pinout (v2.1)

| Function | GPIO |
| --- | --- |
| **I²C** SDA / SCL | IO33 / IO32 |
| **SPI** MISO / MOSI / CLK / CS | IO12 / IO13 / IO14 / IO15 |
| **microSD** MISO / MOSI / CLK / CS / CD | IO12 / IO13 / IO14 / IO2 / IO34 |
| **Ethernet** MDC / MDIO / NRST / CLK | IO23 / IO18 / IO5 / GPIO17 (out) |
| **User LED** (SK6812) | IO0 |
| Input-only pins | IO34, IO35, IO39 |

![ESPlan pinout](img/ESPlan_pinout.png)

### Controls & LED

- **RESET** button – resets the ESP32.
- **IO0 / BOOT** button – enter firmware upload mode (RESET + IO0).
- **Status LED** (SK6812) on IO0 – programmable user LED.

> **Ethernet init (Arduino core 2.x):** the LAN8720A clock is on GPIO17. A known-good call is:
> ```cpp
> ETH.begin(0, -1, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
> ```
> On newer cores use the named-enum signature, e.g. 
> ```cpp
> ETH.begin(ETH_PHY_LAN8720, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLOCK_GPIO17_OUT);
> ```
>
> **I²C init:**
> ```cpp
> Wire.begin(33, 32);
> ```
---

## 📐 Specifications

| | |
| --- | --- |
| **MCU** | ESP32-WROOM-32E (Xtensa LX6 dual-core 240 MHz, 4 MB Flash, Wi-Fi + BT) |
| **Ethernet PHY** | LAN8720A-CP, Fast Ethernet 10/100 Mbps |
| **RS485** | WS3081, half-duplex, TVS protection, optional 120 Ω termination |
| **Power – USB-C** | 5 V |
| **Power – terminal** | 7–40 V DC (12 or 24 V recommended) |
| **Power – PoE** | IEEE 802.3af via DP9900M module (12 V or 24 V variant, sold separately) |
| **Internal rails** | 5 V (buck LMR14050, up to 5 A) → 3.3 V (LDO XC6220) |
| **microSD** | SPI, connector with card detect |
| **Programming** | USB-C with on-board USB-UART bridge (auto-boot), RESET + BOOT buttons |
| **Compatibility** | Arduino IDE, ESP-IDF, ESPHome, Tasmota, MicroPython |

---

## 📦 Package contents

- 1× LaskaKit ESPlan v2.1

The PoE module (SDaPo DP9900M) and the 3D-printed enclosure are **not included** – ordered separately.

---

## 🖨️ 3D-printed enclosure

STL/3MF files for the enclosure are in the [`3D`](3D) folder.

---

## License & support

Open-source hardware by LaskaKit. Questions, issues and pull requests are welcome via the [Issues](../../issues) tab.
