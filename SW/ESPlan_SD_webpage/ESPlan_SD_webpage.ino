/* Webserver SD webpage example for LaskaKit ESPlan
 * Place index.htm file on SD card or create your own html example, rename it to index.htm and place it on SD card
 * After boot connect to http://esplan.local or to IP adress of ESP32 (for example 192.168.0.98)
 * You should see webpage saved on SD card
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>

const char *ssid = "WiFi_6";
const char *password = "WiFiBurian6";
const char *host = "esplan";

// SD card defines
#define SCK 14
#define MISO 12
#define MOSI 13
#define CS 15
SPIClass spi = SPIClass(HSPI);

WebServer server(80);

// LAN8720A parameters
#define ETH_POWER_PIN -1
#define ETH_ADDR 0
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_NRST_PIN 5

static bool eth_connected = false;

void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        // set eth hostname here
        ETH.setHostname("esplan");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex())
        {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
        eth_connected = true;
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        eth_connected = false;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        eth_connected = false;
        break;
    default:
        break;
    }
}

void handleNotFound()
{
    String message = "SD card or index.htm file not detected\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
    Serial.print(message);
}

void handleRoot()
{
    spi.begin(SCK, MISO, MOSI, CS);
    if (SD.begin(CS, spi))
    {
        Serial.println("SD Card initialized.");
    }
    else
    {
        handleNotFound();
        return;
    }
    File dataFile = SD.open("/index.htm");

    if (!dataFile)
    {
        SD.end();
        spi.end();
        handleNotFound();
        return;
    }

    if (server.streamFile(dataFile, "text/html") != dataFile.size())
    {
        Serial.println("Sent less data than expected!");
    }

    dataFile.close();
    SD.end();
    spi.end();
}

void eth_setup()
{
    // For reseting LAN8720A
    pinMode(ETH_NRST_PIN, OUTPUT);
    digitalWrite(ETH_NRST_PIN, LOW);
    delay(500);
    digitalWrite(ETH_NRST_PIN, HIGH);

    WiFi.onEvent(WiFiEvent);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
}

void DNS_setup()
{
    if (MDNS.begin(host))
    {
        MDNS.addService("http", "tcp", 80);
        Serial.println("MDNS responder started");
        Serial.print("You can now connect to http://");
        Serial.print(host);
        Serial.println(".local");
    }
}

void setup(void)
{
    Serial.begin(115200);
    Serial.print("\n");

    eth_setup();
    DNS_setup();

    server.on("/", handleRoot); // Main page
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void loop(void)
{
    server.handleClient();
    delay(2); // allow the cpu to switch to other tasks
}