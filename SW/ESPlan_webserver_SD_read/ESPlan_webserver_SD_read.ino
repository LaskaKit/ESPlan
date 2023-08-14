#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "page.h"

const char *host = "esplan";  // Connect to http://ESPlan.local

// SD card defines
#define SCK 14
#define MISO 12
#define MOSI 13
#define CS 15
SPIClass spi = SPIClass(HSPI);

// Set web server port number to 80
WebServer server(80);

// LAN8720A parameters
#define ETH_POWER_PIN   -1
#define ETH_ADDR        0
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_NRST_PIN    5

static bool eth_connected = false;

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
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
      if (ETH.fullDuplex()) {
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

void readFile(fs::FS &fs, const char *path)
{
  char text[100] = {0};
  char out[100] = {0};
  uint8_t i = 0;
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    text[i] = file.read();
    if(text[i++] == '\n')
    {
      Serial.print(text);
      sprintf(out, "<p>%s</p>", text);
      server.sendContent(out);
      memset(text, 0, 99*sizeof(*text));
      memset(out, 0, 99*sizeof(*out));
      i = 0;
    }
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

uint8_t appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return 1;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
    file.close();
    return 0;
  }
  else
  {
    Serial.println("Append failed");
    file.close();
    return 1;
  }
}

void handleRoot()
{
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send_P(200, "text/html", index_html);
  spi.begin(SCK, MISO, MOSI, CS);
  if (SD.begin(CS, spi))
  {
    Serial.println("SD Card initialized.");
    readFile(SD, "/output.txt");
  }
  else
  {
    server.sendContent("<p style= \"color:rgb(255, 0, 0);\">SD card not found!</p>");
  }
  server.sendContent("</body>");
  server.sendContent(""); // This ends sending
  SD.end();
  spi.end();
}

void handleSubmit()
{
  char text[100] = {0};
  spi.begin(SCK, MISO, MOSI, CS);
  if (SD.begin(CS, spi))
  {
    Serial.println("SD Card initialized.");
    sprintf(text, "%s\n", server.arg(0).c_str());
    Serial.println(server.arg(0));
    if (appendFile(SD, "/output.txt", text))
    {
      writeFile(SD, "/output.txt", text);
    }
  }
  SD.end();
  spi.end();
  handleRoot();
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
  // For reseting LAN8720A
  pinMode(ETH_NRST_PIN, OUTPUT);
  digitalWrite(ETH_NRST_PIN, LOW);
  delay(500);
  digitalWrite(ETH_NRST_PIN, HIGH);

  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);

  DNS_setup();

  server.on("/", handleRoot);   // Main page

  server.on("/get", handleSubmit);  // Function done on GET request

  server.onNotFound(handleNotFound);  // Function done when hangle is not found

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  if(eth_connected)
  {
    server.handleClient();
  }
  delay(2); // allow the cpu to switch to other tasks
}