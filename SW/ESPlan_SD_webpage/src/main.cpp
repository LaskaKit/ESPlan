/* Webserver SD webpage example for LaskaKit ESPlan
 * Place index.htm file on SD card or create your own html example, rename it to index.htm and place it on SD card
 * After boot connect to http://esplan.local or to IP adress of ESP32 (for example 192.168.0.98)
 * You should see webpage saved on SD card
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
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

void wifi_setup()
{
    uint8_t i = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED && i++ < 20)
    {
        Serial.print(".");
        delay(500);
    }
    if (i == 21)
    {
        Serial.print("\n");
        Serial.print("Could not connect to ");
        Serial.println(ssid);
        while (1)
        {
            delay(500);
        }
    }
    Serial.print("\n");
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
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

    wifi_setup();
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