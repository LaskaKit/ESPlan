![ESPlan pinout](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan%20(1).jpg)

# ESPlan
ESP32 má Wi-Fi a Bluetooth rozhraní přímo v sobě, ale někdy je potřeba jej rozšířit o ethernet nebo RS485. Proto jsme vytvořili [ESPlan](https://www.laskakit.cz/laskakit-esplan-esp32-lan8720a-max485-poe/), ten má všechny čtyři vyjmenované periférie na jedné jediné desce - je potřeba komunikovat skrze Wi-Fi? Není problém. Bluetooth? I ten můžeš použít. RS485 pro připojení čidel, která jsou desítky metrů daleko? Samozřejmě! A data odesílat přes ethernet? Jasně! A navíc můžeš desku napájet skrze PoE.

K podpoře PoE potřebuješ [5V PoE modul](https://www.laskakit.cz/sdapo-dp1435-poe-modul-ieee-802-3af-5v-2-4a/), který u nás v e-shopu také najdeš. 

Máme dvě verze ESPlan lišící se typem antény
- s anténou na DPS
- s konektorem pro anténu (výhoda je připojení na delší vzdálenost)

Jako LAN kontrolét jsme použili osvědčený LAN8720A. Tento kontrolér je totiž podporován v HomeAssistant a stejně tak v ESPhome. To znamená, že tuto desku si můžeš připojit přímo do tvé chytré domácnosti nebo dílny. 

Konektor USB-C, který je na desce zajištuje jak napájení (pokud nepoužíváš [PoE modul](https://www.laskakit.cz/sdapo-dp1435-poe-modul-ieee-802-3af-5v-2-4a/)), tak i programování. Programátor je totiž na desce - žádné zbytečné mačkání tlačítek, aby ses dostal do bootloader režimu a mohl programovat - tohle se totiž dělá "samo". 

O napájení celé desky se stará spínaný, s vysokou účinností (max 96%) , měnič SY8089, který z 5V vytvoří stabilní napájení 3.3V/ až 2A.

Na desce, kromě RJ45, RS485 a USB-C, najdeš i náš uŠup konektor pro připojení čidel (teplota, vlhkost, CO2, tlak, ...) a GPIO header s vyvedenými GPIO, aby sis mohl, kdybys chtěl, připojit další zařízení. 

ESPlan koupíš na https://www.laskakit.cz/laskakit-esplan-esp32-lan8720a-max485-poe/

![ESPlan pinout](https://github.com/LaskaKit/ESPlan/blob/main/img/ESPlan_pinout.JPG)
