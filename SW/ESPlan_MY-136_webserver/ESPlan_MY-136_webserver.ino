/* Webserver reads from SD text file test for LaskaKit ESPlan
 * After boot connect to http://esplan.local or to IP adress of ESP32 (for example 192.168.0.98)
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "page.h"

const char *host = "esplan"; // Connect to http://esplan.local

// Set web server port number to 80
WebServer server(80);

// LAN8720A parameters
#define ETH_POWER_PIN -1
#define ETH_ADDR 0
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_NRST_PIN 5

// RS485 parameters
#define RS485_REC_bytes 8
byte readAddr[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
byte readBr[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA};

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

uint16_t calcCRC(const unsigned char *indata, int len)
{
    uint16_t crc = 0xffff;
    uint8_t temp;
    int n;
    while (len--)
    {
        crc = *indata ^ crc;
        for (uint8_t n = 0; n < 8; n++)
        {
            char TT;
            TT = crc & 1;
            crc = crc >> 1;
            crc = crc & 0x7fff;
            if (TT == 1)
            {
                crc = crc ^ 0xa001;
                crc = crc & 0xffff;
            }
        }
        indata++;
    }
    return crc;
}

bool getData(byte d[], int16_t i, uint8_t *data)
{
    int8_t rcv[7];
    uint8_t crc[2];
    uint8_t dat[5];

    if (Serial1.write(d, i) == 8) // Send command
    {
        if (Serial1.available())
        {
            Serial.print("RCV data: ");
            for (uint8_t y = 0; y < 7; y++)
            {
                rcv[y] = Serial1.read();
                if (y == 5)
                    crc[0] = rcv[y];
                if (y == 6)
                    crc[1] = rcv[y];

                char buff[2];
                sprintf(buff, "%02X", rcv[y]);
                Serial.print(buff);
                Serial.print(" ");
            }
        }
        else
        {
            Serial.println("NO DATA RECEIVED!");
            return 1;
        }
    }

    for (int i = 0; i < (sizeof rcv) - 2; i++)
    {
        dat[i] = rcv[i];
    }

    if ((((uint16_t)crc[1] << 8) | (uint16_t)crc[0]) != calcCRC(dat, sizeof(dat))) // Check CRC
    {
        Serial.print(" >> ");
        Serial.print(crc[1], HEX);
        Serial.print(" ");
        Serial.print(crc[0], HEX);
        Serial.print(" = ");
        Serial.println("CRC FAIL!");
        return 1;
    }

    Serial.println();
    Serial.print("DATA: ");
    for (int i = 0; i < sizeof dat; i++)
    {
        char buff[2];
        sprintf(buff, "%02X", dat[i]);
        Serial.print(buff);
        Serial.print(" ");
        data[i] = dat[i];
    }
    Serial.println();
    return 0;
}


void handleRoot()
{
    server.send_P(200, "text/html", index_html); // Send web page
}

void handleNotFound()
{
    String message = "Error\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

void handleSendData()
{
    uint8_t data[5] = {0};
    char buff[20] = {0};

    if (getData(readAddr, RS485_REC_bytes, data))
    {
        server.send(200, "text/plain", "ERROR");
    }
    else
    {
    	sprintf(buff, "%02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
        server.send(200, "text/plain", buff);
    }
}

void rs485_init()
{
    Serial1.begin(9600, SERIAL_8N1, 36, 4); // ESP32 RX1 IO36, TX1 IO4
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

void setup()
{
    Serial.begin(115200);
    rs485_init();
    // For reseting LAN8720A
    pinMode(ETH_NRST_PIN, OUTPUT);
    digitalWrite(ETH_NRST_PIN, LOW);
    delay(500);
    digitalWrite(ETH_NRST_PIN, HIGH);

    WiFi.onEvent(WiFiEvent);

    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);

    DNS_setup();

    server.on("/", handleRoot); // Main page

    server.onNotFound(handleNotFound); // Function done when hangle is not found

    server.on("/readData", handleSendData);

    server.begin();
    Serial.println("HTTP server started");
}

void loop()
{
    if (eth_connected)
    {
        server.handleClient();
    }
    delay(2); // allow the cpu to switch to other tasks
}