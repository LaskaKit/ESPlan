![ESPlan](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan%20(1).jpg)

# ESPlan
The ESP32 has Wi-Fi and Bluetooth interfaces built in the chip, but sometimes it needs to be extended with Ethernet or RS485. That's why we created [the ESPlan](https://www.laskakit.cz/laskakit-esplan-esp32-lan8720a-max485-poe/), it has all four listed peripherals on one single board - do you need to communicate via Wi-Fi? No problem. Bluetooth? You can use that too. RS485 to connect sensors that are tens of meters away? Of course! And send data over Ethernet? Sure! And you can power the board via PoE.

To support PoE you need a [5V PoE module](https://www.laskakit.cz/sdapo-dp1435-poe-modul-ieee-802-3af-5v-2-4a/), which you can also find in our store.

We have two versions of the ESPlan, differing in the type of antenna
- with PCB antenna
- with a connector for the antenna (the advantage is a longer distance connection)

As LAN controller we used the proven LAN8720A. This controller is supported in HomeAssistant and also in ESPhome. This means that you can connect this board directly to your smart home or workshop.

The USB-C connector that is on the board provides both power (unless you are using [a PoE module](https://www.laskakit.cz/sdapo-dp1435-poe-modul-ieee-802-3af-5v-2-4a/)) and programming. In fact, the programmer is on the board - no unnecessary button mashing to get into bootloader mode and program - this is what "does itself".

The power supply of the whole board is taken care of by the switching, high efficiency (max 96%) SY8089 inverter, which creates a stable 3.3V/ up to 2A power supply from 5V.

On the board, in addition to RJ45, RS485 and USB-C, you will also find our usup connector for connecting sensors (temperature, humidity, CO2, pressure, ...) and a GPIO header with GPIO outputs so you can connect other devices if you want.

The ESPlan in on this website https://www.laskakit.cz/laskakit-esplan-esp32-lan8720a-max485-poe/

![ESPlan pinout](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan_pinout.JPG)

![ESPlan](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan%20(2).jpg)
![ESPlan](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan%20(4).jpg)
![ESPlan](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan%20(5).jpg)
