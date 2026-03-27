/* Test for LaskaKit ESPlan V2 ESP32 LAN8720A RS485 PoE
 * ESP32 + LAN8720 + WebServer + SD + RS485 + SHT40 + BMP280 + NeoPixel
 *
 * Funkce:
 *  - web rozhrani pres Ethernet
 *  - stav ETH, SD, SHT40, BMP280, RS485
 *  - ovladani NeoPixel LED
 *  - odesilani textu pres RS485
 *  - prijem dat z RS485
 *  - prochazeni adresaru na SD
 *  - upload / stahnout / smazat souboru na SD
 *  - vytvareni a mazani adresaru na SD
 *  - automaticka reakce na vyndani / vlozeni SD karty
 *  - po vlozeni se SD karta ihned znovu nacte
 *  - SD panel se na webu obnovi jen pri zmene stavu SD karty
 *  - periodicky debug do seriove konzole
 *  - volba jazyka CZ / ENG

 * Board:   kaKit ESPlan V2   https://www.laskakit.cz/laskakit-esplan-esp32-lan8720a-max485-poe/
 *
 * 
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <vector>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include "esp_system.h"
#include "logo_web.h"

// =====================================================
// PINOUT
// =====================================================

// ---------- Ethernet LAN8720 ----------
#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_PHY_ADDR   0
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_POWER  -1
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

// ---------- RS485 ----------
#define RS485_TX_PIN   4
#define RS485_RX_PIN   36

// ---------- I2C ----------
#define I2C_SDA_PIN    33
#define I2C_SCL_PIN    32

// ---------- BMP280 ----------
#define BMP280_ADDR_1  0x76
#define BMP280_ADDR_2  0x77

// ---------- SD karta ----------
#define SD_MISO_PIN    12
#define SD_MOSI_PIN    13
#define SD_SCK_PIN     14
#define SD_CS_PIN      2
#define SD_CD_PIN      34
#define SD_CD_ACTIVE_STATE HIGH

// ---------- NeoPixel ----------
#define NEOPIXEL_PIN   0
#define NEOPIXEL_COUNT 1

// =====================================================
// JAZYK UI
// =====================================================

enum UiLang {
  LANG_CZ,
  LANG_EN
};

UiLang currentUiLang = LANG_CZ;

// =====================================================
// STRUKTUROVANÝ LOG
// =====================================================

struct LogEntry {
  uint8_t type;
  String a;
  String b;
  String c;
};

enum LogType : uint8_t {
  LOG_TEXT = 0,
  LOG_SHT40_READ_FAILED,
  LOG_SHT40_NOT_FOUND,
  LOG_SHT40_DETECTED,
  LOG_BMP280_READ_FAILED,
  LOG_BMP280_DETECTED_76,
  LOG_BMP280_DETECTED_77,
  LOG_BMP280_NOT_FOUND,
  LOG_I2C_INIT,
  LOG_SD_NOT_INSERTED,
  LOG_SD_INIT,
  LOG_SD_BEGIN_FAILED,
  LOG_SD_NOT_FOUND,
  LOG_SD_OK,
  LOG_SD_SIZE_MB,
  LOG_SD_REMOVED,
  LOG_SD_INSERTED,
  LOG_RS485_INIT,
  LOG_RS485_PINS,
  LOG_ETH_STATUS_TITLE,
  LOG_ETH_STATUS_LINK,
  LOG_ETH_STATUS_IP,
  LOG_ETH_STATUS_MAC,
  LOG_ETH_START,
  LOG_ETH_CONNECTED,
  LOG_ETH_GOT_IP,
  LOG_ETH_DISCONNECTED,
  LOG_ETH_STOP,
  LOG_ETH_INIT,
  LOG_ETH_BEGIN_FAILED,
  LOG_ETH_OK,
  LOG_ETH_NO_IP,
  LOG_LED_SET,
  LOG_RS485_TX,
  LOG_SD_DELETE_OK,
  LOG_SD_DELETE_ERROR,
  LOG_MKDIR_INVALID,
  LOG_MKDIR_EXISTS,
  LOG_MKDIR_OK,
  LOG_MKDIR_ERROR,
  LOG_UPLOAD_INVALID_NAME,
  LOG_UPLOAD_START,
  LOG_UPLOAD_OPEN_WRITE_FAILED,
  LOG_UPLOAD_FINISHED,
  LOG_UPLOAD_ABORTED,
  LOG_WEB_STARTED,
  LOG_START,
  LOG_READY,
  LOG_OPEN_BROWSER,
  LOG_ETH_NO_IP_YET,
  LOG_RS485_RX,
  LOG_HEAP_TOTAL,
  LOG_HEAP_FREE,
  LOG_CPU_FREQ
};

std::vector<LogEntry> eventLog;

// =====================================================
// GLOBÁLNÍ OBJEKTY
// =====================================================

WebServer server(80);
SPIClass sdSPI(VSPI);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_BMP280 bmp280;
Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool ethConnected = false;
bool ethGotIP = false;
bool sdOk = false;
bool sht40Found = false;
bool bmp280Found = false;
bool sdCardInserted = false;

float lastTemperature = NAN;
float lastHumidity = NAN;
float lastBmpTemperature = NAN;
float lastPressure_hPa = NAN;

uint8_t ledR = 0;
uint8_t ledG = 0;
uint8_t ledB = 0;
uint8_t ledBrightness = 50; // 0-255

String rs485LastSent = "";
String rs485LastReceived = "";
uint32_t rs485RxVersion = 0;
uint32_t rs485Baudrate = 9600;

File uploadFile;

unsigned long lastSensorRead = 0;
unsigned long lastStatusPrint = 0;

// SD detect debounce / hotplug
bool lastSdDetectRaw = false;
bool lastSdDetectStable = false;
unsigned long lastSdDetectChangeMs = 0;

// SD state version for web refresh
uint32_t sdStateVersion = 0;

// Heap baseline = total RAM chip heap
uint32_t totalHeapBytes = 0;

// =====================================================
// JAZYK HELPERY
// =====================================================

UiLang parseLang(const String& s) {
  String v = s;
  v.toLowerCase();
  return (v == "en") ? LANG_EN : LANG_CZ;
}

String langCode(UiLang lang) {
  return (lang == LANG_EN) ? "en" : "cz";
}

UiLang getCurrentLang() {
  return parseLang(server.arg("lang"));
}

void updateCurrentLangFromRequest() {
  if (server.hasArg("lang")) {
    currentUiLang = getCurrentLang();
  }
}

String tr(UiLang lang, const String& cz, const String& en) {
  return (lang == LANG_EN) ? en : cz;
}

String trCurrent(const String& cz, const String& en) {
  return tr(currentUiLang, cz, en);
}

// =====================================================
// LOG
// =====================================================

String renderLogEntry(const LogEntry& e, UiLang lang) {
  switch (e.type) {
    case LOG_TEXT:
      return e.a;

    case LOG_SHT40_READ_FAILED:
      return tr(lang, "CHYBA: čtení SHT40 selhalo", "ERROR: SHT40 read failed");

    case LOG_SHT40_NOT_FOUND:
      return tr(lang, "SHT40 nebyl nalezen", "SHT40 not found");

    case LOG_SHT40_DETECTED:
      return tr(lang, "SHT40 detekován OK", "SHT40 detected OK");

    case LOG_BMP280_READ_FAILED:
      return tr(lang, "CHYBA: čtení BMP280 selhalo", "ERROR: BMP280 read failed");

    case LOG_BMP280_DETECTED_76:
      return tr(lang, "BMP280 detekován na adrese 0x76", "BMP280 detected at address 0x76");

    case LOG_BMP280_DETECTED_77:
      return tr(lang, "BMP280 detekován na adrese 0x77", "BMP280 detected at address 0x77");

    case LOG_BMP280_NOT_FOUND:
      return tr(lang, "BMP280 nebyl nalezen", "BMP280 not found");

    case LOG_I2C_INIT:
      return tr(lang, "Inicializace I2C...", "Initializing I2C...");

    case LOG_SD_NOT_INSERTED:
      return tr(lang, "SD karta není vložena", "SD card is not inserted");

    case LOG_SD_INIT:
      return tr(lang, "Inicializace SD karty...", "Initializing SD card...");

    case LOG_SD_BEGIN_FAILED:
      return tr(lang, "CHYBA: SD.begin() selhalo", "ERROR: SD.begin() failed");

    case LOG_SD_NOT_FOUND:
      return tr(lang, "CHYBA: SD karta nenalezena", "ERROR: SD card not found");

    case LOG_SD_OK:
      return tr(lang, "SD karta OK", "SD card OK");

    case LOG_SD_SIZE_MB:
      return tr(lang, "Velikost karty: ", "Card size: ") + e.a + " MB";

    case LOG_SD_REMOVED:
      return tr(lang, "SD karta byla vyjmuta", "SD card was removed");

    case LOG_SD_INSERTED:
      return tr(lang, "SD karta byla vložena", "SD card was inserted");

    case LOG_RS485_INIT:
      return tr(lang, "RS485 inicializováno", "RS485 initialized");

    case LOG_RS485_PINS:
      return "TX=" + e.a + ", RX=" + e.b + ", baud=" + e.c;

    case LOG_ETH_STATUS_TITLE:
      return tr(lang, "Ethernet stav:", "Ethernet status:");

    case LOG_ETH_STATUS_LINK:
      return "  " + tr(lang, "Link: ", "Link: ") + e.a;

    case LOG_ETH_STATUS_IP:
      return "  IP: " + e.a;

    case LOG_ETH_STATUS_MAC:
      return "  MAC: " + e.a;

    case LOG_ETH_START:
      return "[ETH] " + tr(lang, "START", "START");

    case LOG_ETH_CONNECTED:
      return "[ETH] " + tr(lang, "PŘIPOJENO", "CONNECTED");

    case LOG_ETH_GOT_IP:
      return "[ETH] " + tr(lang, "ZÍSKÁNA IP: ", "GOT IP: ") + e.a;

    case LOG_ETH_DISCONNECTED:
      return "[ETH] " + tr(lang, "ODPOJENO", "DISCONNECTED");

    case LOG_ETH_STOP:
      return "[ETH] " + tr(lang, "STOP", "STOP");

    case LOG_ETH_INIT:
      return tr(lang, "Inicializace Ethernetu...", "Initializing Ethernet...");

    case LOG_ETH_BEGIN_FAILED:
      return tr(lang, "CHYBA: ETH.begin() selhalo", "ERROR: ETH.begin() failed");

    case LOG_ETH_OK:
      return tr(lang, "Ethernet OK", "Ethernet OK");

    case LOG_ETH_NO_IP:
      return tr(lang, "Ethernet bez IP adresy", "Ethernet without IP address");

    case LOG_LED_SET:
      return tr(lang, "LED nastavena: ", "LED set: ") + "R=" + e.a + " G=" + e.b + " B=" + e.c;

    case LOG_RS485_TX:
      return "RS485 TX: " + e.a;

    case LOG_SD_DELETE_OK:
      return tr(lang, "SD smazání OK: ", "SD delete OK: ") + e.a;

    case LOG_SD_DELETE_ERROR:
      return tr(lang, "SD smazání CHYBA: ", "SD delete ERROR: ") + e.a;

    case LOG_MKDIR_INVALID:
      return tr(lang, "MKDIR: neplatný název adresáře", "MKDIR: invalid directory name");

    case LOG_MKDIR_EXISTS:
      return tr(lang, "MKDIR: adresář již existuje: ", "MKDIR: directory already exists: ") + e.a;

    case LOG_MKDIR_OK:
      return "MKDIR OK: " + e.a;

    case LOG_MKDIR_ERROR:
      return tr(lang, "MKDIR CHYBA: ", "MKDIR ERROR: ") + e.a;

    case LOG_UPLOAD_INVALID_NAME:
      return tr(lang, "Upload: neplatné jméno souboru", "Upload: invalid file name");

    case LOG_UPLOAD_START:
      return tr(lang, "Upload start: ", "Upload start: ") + e.a;

    case LOG_UPLOAD_OPEN_WRITE_FAILED:
      return tr(lang, "Upload: nelze otevřít soubor pro zápis", "Upload: cannot open file for writing");

    case LOG_UPLOAD_FINISHED:
      return tr(lang, "Upload dokončen: ", "Upload finished: ") + e.a + " B";

    case LOG_UPLOAD_ABORTED:
      return tr(lang, "Upload přerušen", "Upload aborted");

    case LOG_WEB_STARTED:
      return tr(lang, "Web server spuštěn na portu 80", "Web server started on port 80");

    case LOG_START:
      return tr(lang, "START", "START");

    case LOG_READY:
      return tr(lang, "PŘIPRAVENO", "READY");

    case LOG_OPEN_BROWSER:
      return tr(lang, "Otevři v prohlížeči: http://", "Open in browser: http://") + e.a + "/";

    case LOG_ETH_NO_IP_YET:
      return tr(lang, "Ethernet zatím nemá IP adresu", "Ethernet does not have an IP address yet");

    case LOG_RS485_RX:
      return "RS485 RX: " + e.a;

    case LOG_HEAP_TOTAL:
      return tr(lang, "Heap celkem: ", "Heap total: ") + e.a + " B";

    case LOG_HEAP_FREE:
      return tr(lang, "Heap volný: ", "Heap free: ") + e.a + " B (" + e.b + " %)";

    case LOG_CPU_FREQ:
      return "CPU: " + e.a + " MHz";

    default:
      return e.a;
  }
}

void pushLogEntry(uint8_t type, const String& a = "", const String& b = "", const String& c = "") {
  LogEntry e{type, a, b, c};
  eventLog.push_back(e);

  const size_t maxEntries = 250;
  if (eventLog.size() > maxEntries) {
    eventLog.erase(eventLog.begin(), eventLog.begin() + (eventLog.size() - maxEntries));
  }
}

void addLogEntry(uint8_t type, const String& a = "", const String& b = "", const String& c = "") {
  LogEntry e{type, a, b, c};
  Serial.println(renderLogEntry(e, currentUiLang));
  pushLogEntry(type, a, b, c);
}

void addLogText(const String& msg) {
  Serial.println(msg);
  pushLogEntry(LOG_TEXT, msg);
}

// =====================================================
// RESET REASON
// =====================================================

void printResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();

  Serial.print("Reset reason: ");
  switch (reason) {
    case ESP_RST_UNKNOWN:   Serial.println("UNKNOWN"); break;
    case ESP_RST_POWERON:   Serial.println("POWERON"); break;
    case ESP_RST_EXT:       Serial.println("EXTERNAL"); break;
    case ESP_RST_SW:        Serial.println("SOFTWARE"); break;
    case ESP_RST_PANIC:     Serial.println("PANIC"); break;
    case ESP_RST_INT_WDT:   Serial.println("INT_WDT"); break;
    case ESP_RST_TASK_WDT:  Serial.println("TASK_WDT"); break;
    case ESP_RST_WDT:       Serial.println("WDT"); break;
    case ESP_RST_DEEPSLEEP: Serial.println("DEEPSLEEP"); break;
    case ESP_RST_BROWNOUT:  Serial.println("BROWNOUT"); break;
    case ESP_RST_SDIO:      Serial.println("SDIO"); break;
    default:                Serial.println((int)reason); break;
  }
}

// =====================================================
// POMOCNÉ FUNKCE
// =====================================================

String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '\\': out += F("\\\\"); break;
      case '"': out += F("\\\""); break;
      case '\b': out += F("\\b"); break;
      case '\f': out += F("\\f"); break;
      case '\n': out += F("\\n"); break;
      case '\r': out += F("\\r"); break;
      case '\t': out += F("\\t"); break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04X", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

String urlEncode(const String& s) {
  String out;
  char hex[4];
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
      out += (char)c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      out += hex;
    }
  }
  return out;
}

String formatBytes(uint64_t bytes) {
  if (bytes < 1024ULL) return String((uint32_t)bytes) + " B";
  if (bytes < 1024ULL * 1024ULL) return String((float)bytes / 1024.0f, 1) + " kB";
  if (bytes < 1024ULL * 1024ULL * 1024ULL) return String((float)bytes / 1024.0f / 1024.0f, 1) + " MB";
  return String((float)bytes / 1024.0f / 1024.0f / 1024.0f, 1) + " GB";
}

String normalizePath(String path) {
  path.trim();
  if (path.length() == 0) return "/";
  if (!path.startsWith("/")) path = "/" + path;
  while (path.indexOf("//") >= 0) path.replace("//", "/");
  return path;
}

String normalizeDirPath(String path) {
  path = normalizePath(path);
  if (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }
  return path;
}

bool isSafePath(String path) {
  if (!path.startsWith("/")) return false;
  if (path.indexOf("..") >= 0) return false;
  if (path.indexOf('\\') >= 0) return false;
  return true;
}

String parentPath(String path) {
  path = normalizeDirPath(path);
  if (path == "/") return "/";

  int lastSlash = path.lastIndexOf('/');
  if (lastSlash <= 0) return "/";
  return path.substring(0, lastSlash);
}

String joinPath(String base, String name) {
  base = normalizeDirPath(base);
  if (base != "/" && base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  while (name.startsWith("/")) {
    name.remove(0, 1);
  }
  if (base == "/") return "/" + name;
  return base + "/" + name;
}

bool deleteRecursive(const String& path) {
  if (!SD.exists(path)) return false;

  File entry = SD.open(path);
  if (!entry) return false;

  bool isDir = entry.isDirectory();
  entry.close();

  if (!isDir) {
    return SD.remove(path);
  }

  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }

  File child = dir.openNextFile();
  while (child) {
    String childPath = normalizePath(String(child.name()));
    bool childIsDir = child.isDirectory();
    child.close();

    if (childIsDir) {
      if (!deleteRecursive(childPath)) {
        dir.close();
        return false;
      }
    } else {
      if (!SD.remove(childPath)) {
        dir.close();
        return false;
      }
    }

    yield();
    child = dir.openNextFile();
  }

  dir.close();
  return SD.rmdir(path);
}

String contentTypeFromFilename(const String& filename) {
  String lower = filename;
  lower.toLowerCase();

  if (lower.endsWith(".htm") || lower.endsWith(".html")) return "text/html";
  if (lower.endsWith(".css")) return "text/css";
  if (lower.endsWith(".js")) return "application/javascript";
  if (lower.endsWith(".txt")) return "text/plain";
  if (lower.endsWith(".log")) return "text/plain";
  if (lower.endsWith(".json")) return "application/json";
  if (lower.endsWith(".csv")) return "text/csv";
  if (lower.endsWith(".xml")) return "application/xml";
  if (lower.endsWith(".pdf")) return "application/pdf";
  if (lower.endsWith(".zip")) return "application/zip";
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
  if (lower.endsWith(".png")) return "image/png";
  if (lower.endsWith(".gif")) return "image/gif";
  return "application/octet-stream";
}

String colorToHex(uint8_t r, uint8_t g, uint8_t b) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

float getFreeHeapPercent() {
  if (totalHeapBytes == 0) return 0.0f;
  return ((float)ESP.getFreeHeap() * 100.0f) / (float)totalHeapBytes;
}

void bumpSdStateVersion() {
  sdStateVersion++;
}

void setPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  ledR = r;
  ledG = g;
  ledB = b;

  pixel.setBrightness(ledBrightness);
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void redirectHome(UiLang lang) {
  server.sendHeader("Location", "/?lang=" + langCode(lang));
  server.send(303);
}

void redirectToPath(const String& path, UiLang lang) {
  server.sendHeader(
    "Location",
    "/?path=" + urlEncode(normalizeDirPath(path)) + "&lang=" + langCode(lang)
  );
  server.send(303);
}

// =====================================================
// SHT40
// =====================================================

void readSHT40() {
  if (!sht40Found) return;

  sensors_event_t humidity, temp;
  if (sht4.getEvent(&humidity, &temp)) {
    lastTemperature = temp.temperature;
    lastHumidity = humidity.relative_humidity;
  } else {
    addLogEntry(LOG_SHT40_READ_FAILED);
  }
}

void initSHT40() {
  if (!sht4.begin(&Wire)) {
    sht40Found = false;
    addLogEntry(LOG_SHT40_NOT_FOUND);
    return;
  }

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);
  sht40Found = true;
  addLogEntry(LOG_SHT40_DETECTED);
  readSHT40();
}

// =====================================================
// BMP280
// =====================================================

void readBMP280() {
  if (!bmp280Found) return;

  float t = bmp280.readTemperature();
  float p = bmp280.readPressure();

  if (isnan(t) || isnan(p) || p <= 0.0f) {
    addLogEntry(LOG_BMP280_READ_FAILED);
    return;
  }

  lastBmpTemperature = t;
  lastPressure_hPa = p / 100.0f;
}

void initBMP280() {
  bool ok = false;

  if (bmp280.begin(BMP280_ADDR_1)) {
    ok = true;
    addLogEntry(LOG_BMP280_DETECTED_76);
  } else if (bmp280.begin(BMP280_ADDR_2)) {
    ok = true;
    addLogEntry(LOG_BMP280_DETECTED_77);
  }

  if (!ok) {
    bmp280Found = false;
    addLogEntry(LOG_BMP280_NOT_FOUND);
    return;
  }

  bmp280.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );

  bmp280Found = true;
  readBMP280();
}

// =====================================================
// I2C senzory
// =====================================================

void initI2CDevices() {
  addLogEntry(LOG_I2C_INIT);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(50);

  initSHT40();
  initBMP280();
}

// =====================================================
// SD KARTA
// =====================================================

bool isSdCardPresent() {
  pinMode(SD_CD_PIN, INPUT);
  return digitalRead(SD_CD_PIN) == SD_CD_ACTIVE_STATE;
}

void initSDCard() {
  sdCardInserted = isSdCardPresent();

  if (!sdCardInserted) {
    sdOk = false;
    addLogEntry(LOG_SD_NOT_INSERTED);
    bumpSdStateVersion();
    return;
  }

  addLogEntry(LOG_SD_INIT);

  SD.end();
  sdSPI.end();
  delay(10);

  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, sdSPI)) {
    sdOk = false;
    addLogEntry(LOG_SD_BEGIN_FAILED);
    bumpSdStateVersion();
    return;
  }

  if (SD.cardType() == CARD_NONE) {
    sdOk = false;
    addLogEntry(LOG_SD_NOT_FOUND);
    SD.end();
    sdSPI.end();
    bumpSdStateVersion();
    return;
  }

  sdOk = true;
  addLogEntry(LOG_SD_OK);
  addLogEntry(LOG_SD_SIZE_MB, String((uint32_t)(SD.cardSize() / (1024ULL * 1024ULL))));
  bumpSdStateVersion();
}

void handleSdHotplug() {
  bool raw = isSdCardPresent();

  if (raw != lastSdDetectRaw) {
    lastSdDetectRaw = raw;
    lastSdDetectChangeMs = millis();
  }

  if ((millis() - lastSdDetectChangeMs) < 150) {
    return;
  }

  if (raw == lastSdDetectStable) {
    return;
  }

  lastSdDetectStable = raw;
  sdCardInserted = raw;

  if (!raw) {
    if (uploadFile) {
      uploadFile.close();
    }

    SD.end();
    sdSPI.end();
    sdOk = false;
    addLogEntry(LOG_SD_REMOVED);
    bumpSdStateVersion();
    return;
  }

  addLogEntry(LOG_SD_INSERTED);
  delay(50);
  initSDCard();
}

// =====================================================
// SD PANEL
// =====================================================

String buildFileTable(const String& currentPath, UiLang lang) {
  if (!sdOk) {
    return String(F("<p>")) + tr(lang, "SD karta není dostupná.", "SD card is not available.") + F("</p>");
  }

  String path = normalizeDirPath(currentPath);
  if (!isSafePath(path)) path = "/";

  String fallbackPath = parentPath(path);

  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();

    String html;
    html += F("<p><b>");
    html += tr(lang, "Chyba:", "Error:");
    html += F("</b> ");
    html += tr(lang, "Nelze otevřít adresář.", "Cannot open directory.");
    html += F("</p>");

    html += F("<p><b>");
    html += tr(lang, "Požadovaná cesta:", "Requested path:");
    html += F("</b> ");
    html += htmlEscape(path);
    html += F("</p>");

    html += F("<p><a href='/?path=");
    html += urlEncode(fallbackPath);
    html += F("&lang=");
    html += langCode(lang);
    html += F("'><span style='font-size:24px;'>↩️</span></a></p>");

    return html;
  }

  String html;
  html += F("<p><b>");
  html += tr(lang, "Aktuální adresář:", "Current directory:");
  html += F("</b> ");
  html += htmlEscape(path);
  html += F("</p>");

  html += F("<form method='GET' action='/'><label>");
  html += tr(lang, "Přejít do adresáře", "Go to directory");
  html += F("</label><input type='text' name='path' value='");
  html += htmlEscape(path);
  html += F("'><input type='hidden' name='lang' value='");
  html += langCode(lang);
  html += F("'><button type='submit'>");
  html += tr(lang, "Otevřít", "Open");
  html += F("</button></form>");

  if (path != "/") {
    html += F("<p><a href='/?path=");
    html += urlEncode(parentPath(path));
    html += F("&lang=");
    html += langCode(lang);
    html += F("'><span style='font-size:24px;'>↩️</span></a></p>");
  }

  html += F("<table><tr><th>");
  html += tr(lang, "Typ", "Type");
  html += F("</th><th>");
  html += tr(lang, "Název", "Name");
  html += F("</th><th>");
  html += tr(lang, "Velikost", "Size");
  html += F("</th><th>");
  html += tr(lang, "Akce", "Actions");
  html += F("</th></tr>");

  File file = dir.openNextFile();
  bool any = false;

  while (file) {
    any = true;

    String childName = String(file.name());
    int slashPos = childName.lastIndexOf('/');
    if (slashPos >= 0) {
      childName = childName.substring(slashPos + 1);
    }

    String entryPath = joinPath(path, childName);
    String displayName = childName;

    if (displayName.startsWith(path) && path != "/") {
      displayName = displayName.substring(path.length());
      if (!displayName.startsWith("/")) displayName = "/" + displayName;
    }

    if (displayName.startsWith("/")) displayName.remove(0, 1);
    if (displayName.length() == 0) displayName = "(bez názvu)";

    bool isDir = file.isDirectory();
    uint64_t size = isDir ? 0 : file.size();

    String entryEsc = htmlEscape(displayName);
    String entryUrl = urlEncode(entryPath);
    String currentUrl = urlEncode(path);

    html += F("<tr><td>");
    html += isDir ? "📁" : "📄";
    html += F("</td><td>");

    if (isDir) {
      html += F("<a href='/?path=");
      html += entryUrl;
      html += F("&lang=");
      html += langCode(lang);
      html += F("'>");
      html += entryEsc;
      html += F("</a>");
    } else {
      html += entryEsc;
    }

    html += F("</td><td>");
    html += isDir ? "-" : formatBytes(size);
    html += F("</td><td>");

    if (!isDir) {
      html += F("<a href='/download?name=");
      html += entryUrl;
      html += F("&lang=");
      html += langCode(lang);
      html += F("'>⬇️</a> | ");
    }

    html += F("<a href='/delete?name=");
    html += entryUrl;
    html += F("&path=");
    html += currentUrl;
    html += F("&lang=");
    html += langCode(lang);
    html += F("' onclick=\"return confirm('");
    html += tr(lang, "Opravdu smazat?", "Really delete?");
    html += F("')\">🗑️</a>");

    html += F("</td></tr>");

    yield();
    file = dir.openNextFile();
  }

  if (!any) {
    html += F("<tr><td colspan='4'>");
    html += tr(lang, "Adresář je prázdný", "Directory is empty");
    html += F("</td></tr>");
  }

  html += F("</table>");
  dir.close();
  return html;
}

String buildSdPanel(const String& currentPath, UiLang lang) {
  String html;
  html.reserve(18000);

  html += F("<h2>");
  html += tr(lang, "Správa souborů na SD", "SD file manager");
  html += F("</h2>");

  if (sdOk) {
    html += F("<div class='tools'>");

    html += F("<div class='toolbox'><h3>");
    html += tr(lang, "Nahrávání souborů", "File upload");
    html += F("</h3>"
              "<form method='POST' action='/upload' enctype='multipart/form-data'>"
              "<input type='hidden' name='path' value='");
    html += htmlEscape(currentPath);
    html += F("'>"
              "<input type='hidden' name='lang' value='");
    html += langCode(lang);
    html += F("'>"
              "<input type='file' name='data'>"
              "<button type='submit'>");
    html += tr(lang, "Nahrát", "Upload");
    html += F("</button>"
              "</form></div>");

    html += F("<div class='toolbox'><h3>");
    html += tr(lang, "Vytvořit adresář", "Create directory");
    html += F("</h3>"
              "<form method='GET' action='/mkdir'>"
              "<input type='hidden' name='path' value='");
    html += htmlEscape(currentPath);
    html += F("'>"
              "<input type='hidden' name='lang' value='");
    html += langCode(lang);
    html += F("'>"
              "<label>");
    html += tr(lang, "Název nové složky", "New folder name");
    html += F("</label>"
              "<input type='text' name='name'>"
              "<button type='submit'>");
    html += tr(lang, "Vytvořit", "Create");
    html += F("</button>"
              "</form></div>");

    html += F("</div>");
  } else {
    html += F("<p>");
    html += tr(lang, "SD karta není dostupná.", "SD card is not available.");
    html += F("</p>");
  }

  html += buildFileTable(currentPath, lang);
  return html;
}

String buildSdStateJson() {
  String json = "{";
  json += "\"inserted\":";
  json += (sdCardInserted ? "true" : "false");
  json += ",\"ok\":";
  json += (sdOk ? "true" : "false");
  json += ",\"version\":";
  json += String(sdStateVersion);
  json += "}";
  return json;
}

// =====================================================
// RS485
// =====================================================

bool isSupportedRs485Baud(uint32_t baud) {
  switch (baud) {
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
      return true;
    default:
      return false;
  }
}

void applyRS485Baudrate(uint32_t baud) {
  if (!isSupportedRs485Baud(baud)) return;
  if (rs485Baudrate == baud) return;

  Serial2.flush();
  Serial2.end();
  rs485Baudrate = baud;
  Serial2.begin(rs485Baudrate, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  delay(50);

  addLogText("RS485 baud changed to " + String(rs485Baudrate));
}

void initRS485() {
  Serial2.begin(rs485Baudrate, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  delay(100);
  addLogEntry(LOG_RS485_INIT);
  addLogEntry(LOG_RS485_PINS, String(RS485_TX_PIN), String(RS485_RX_PIN), String(rs485Baudrate));
}

// =====================================================
// ETHERNET
// =====================================================

void printETHStatus() {
  addLogEntry(LOG_ETH_STATUS_TITLE);
  addLogEntry(LOG_ETH_STATUS_LINK, ethConnected ? "UP" : "DOWN");
  addLogEntry(LOG_ETH_STATUS_IP, ethGotIP ? ETH.localIP().toString() : trCurrent("není", "none"));
  if (ethConnected || ethGotIP) {
    addLogEntry(LOG_ETH_STATUS_MAC, ETH.macAddress());
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      addLogEntry(LOG_ETH_START);
      ETH.setHostname("esp32-control");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      addLogEntry(LOG_ETH_CONNECTED);
      ethConnected = true;
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      addLogEntry(LOG_ETH_GOT_IP, ETH.localIP().toString());
      ethGotIP = true;
      printETHStatus();
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      addLogEntry(LOG_ETH_DISCONNECTED);
      ethConnected = false;
      ethGotIP = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      addLogEntry(LOG_ETH_STOP);
      ethConnected = false;
      ethGotIP = false;
      break;

    default:
      break;
  }
}

void initEthernet() {
  addLogEntry(LOG_ETH_INIT);
  WiFi.onEvent(WiFiEvent);

  bool ok = ETH.begin(
    ETH_PHY_TYPE,
    ETH_PHY_ADDR,
    ETH_PHY_MDC,
    ETH_PHY_MDIO,
    ETH_PHY_POWER,
    ETH_CLK_MODE
  );

  if (!ok) {
    addLogEntry(LOG_ETH_BEGIN_FAILED);
    return;
  }

  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (ethGotIP) {
      addLogEntry(LOG_ETH_OK);
      return;
    }
    delay(100);
  }

  addLogEntry(LOG_ETH_NO_IP);
}

// =====================================================
// HTML
// =====================================================

String buildStatusPanel(UiLang lang) {
  String ip = ethGotIP ? ETH.localIP().toString() : tr(lang, "není", "none");
  String mac = (ethConnected || ethGotIP) ? ETH.macAddress() : "-";
  String temp = (sht40Found && !isnan(lastTemperature)) ? String(lastTemperature, 2) + " &deg;C" : tr(lang, "není", "none");
  String hum = (sht40Found && !isnan(lastHumidity)) ? String(lastHumidity, 2) + " %RH" : tr(lang, "není", "none");
  String bmpTemp = (bmp280Found && !isnan(lastBmpTemperature)) ? String(lastBmpTemperature, 2) + " &deg;C" : tr(lang, "není", "none");
  String pressure = (bmp280Found && !isnan(lastPressure_hPa)) ? String(lastPressure_hPa, 2) + " hPa" : tr(lang, "není", "none");

  String html;
  html.reserve(3000);

  html += F("<h2>");
  html += tr(lang, "Stav systému", "System status");
  html += F("</h2>");

  html += F("<p><b>");
  html += tr(lang, "Ethernet link:", "Ethernet link:");
  html += F("</b> ");
  html += (ethConnected ? "<span class='ok'>UP</span>" : "<span class='bad'>DOWN</span>");

  html += F("</p><p><b>IP:</b> ");
  html += ip;

  html += F("</p><p><b>MAC:</b> ");
  html += mac;

  html += F("</p><p><b>");
  html += tr(lang, "SD karta:", "SD card:");
  html += F("</b> ");
  html += (sdOk ? "<span class='ok'>OK</span>" : "<span class='bad'>" + tr(lang, "CHYBA", "ERROR") + "</span>");

  html += F("</p><p><b>");
  html += tr(lang, "SD detect:", "SD detect:");
  html += F("</b> ");
  html += (sdCardInserted ? "<span class='ok'>" + tr(lang, "vložena", "inserted") + "</span>"
                          : "<span class='bad'>" + tr(lang, "není vložena", "not inserted") + "</span>");

  html += F("</p><p><b>SHT40:</b> ");
  html += (sht40Found ? "<span class='ok'>OK</span>" : "<span class='bad'>" + tr(lang, "není nalezen", "not found") + "</span>");

  html += F("</p><p><b>");
  html += tr(lang, "SHT40 teplota:", "SHT40 temperature:");
  html += F("</b> ");
  html += temp;

  html += F("</p><p><b>");
  html += tr(lang, "Vlhkost:", "Humidity:");
  html += F("</b> ");
  html += hum;

  html += F("</p><p><b>BMP280:</b> ");
  html += (bmp280Found ? "<span class='ok'>OK</span>" : "<span class='bad'>" + tr(lang, "není nalezen", "not found") + "</span>");

  html += F("</p><p><b>");
  html += tr(lang, "BMP280 teplota:", "BMP280 temperature:");
  html += F("</b> ");
  html += bmpTemp;

  html += F("</p><p><b>");
  html += tr(lang, "Tlak:", "Pressure:");
  html += F("</b> ");
  html += pressure;

  html += F("</p><p><b>FreeHeap:</b> ");
  html += String(ESP.getFreeHeap());
  html += F(" B (");
  html += String(getFreeHeapPercent(), 1);
  html += F(" %)</p><p><b>");
  html += tr(lang, "Heap celkem:", "Total heap:");
  html += F("</b> ");
  html += String(totalHeapBytes);
  html += F(" B</p>");

  html += F("<p><a href='/?lang=");
  html += langCode(lang);
  html += F("'>");
  html += tr(lang, "Obnovit celou stránku", "Reload full page");
  html += F("</a></p>");

  return html;
}

String buildNeoPixelPanel(UiLang lang) {
  String html;
  html.reserve(7500);

  html += F("<h2>NeoPixel</h2>");

  html += F("<form action='/led' method='get' id='ledForm'>");
  html += F("<input type='hidden' name='lang' value='");
  html += langCode(lang);
  html += F("'>");

  html += F("<label>");
  html += tr(lang, "Vyberte barvu", "Select color");
  html += F("</label>");
  html += F("<div style='background:#f8fafc;border:1px solid #dbe3ee;border-radius:12px;padding:10px;margin-bottom:12px;'>");

  html += F("<div id='ledColorArea' style='position:relative;width:100%;height:190px;border-radius:10px;overflow:hidden;cursor:crosshair;border:1px solid #cfd8e3;background:red;'>");
  html += F("<div style='position:absolute;inset:0;background:linear-gradient(to right,#fff,rgba(255,255,255,0));'></div>");
  html += F("<div style='position:absolute;inset:0;background:linear-gradient(to top,black,rgba(0,0,0,0));'></div>");
  html += F("<div id='ledColorCursor' style='position:absolute;width:14px;height:14px;border:2px solid #fff;border-radius:50%;box-shadow:0 0 0 1px rgba(0,0,0,.5);pointer-events:none;transform:translate(-7px,-7px);left:100%;top:0;'></div>");
  html += F("</div>");

  html += F("<label style='margin-top:10px;'>");
  html += tr(lang, "Odstín", "Hue");
  html += F("</label>");
  html += F("<div id='ledHueWrap' style='position:relative;height:18px;border-radius:999px;overflow:hidden;border:1px solid #cfd8e3;cursor:pointer;background:linear-gradient(to right,#ff0000,#ffff00,#00ff00,#00ffff,#0000ff,#ff00ff,#ff0000);'>");
  html += F("<div id='ledHueCursor' style='position:absolute;top:0;width:14px;height:18px;border:2px solid #fff;box-shadow:0 0 0 1px rgba(0,0,0,.5);background:transparent;pointer-events:none;transform:translateX(-7px);left:0;'></div>");
  html += F("</div>");

  html += F("<input type='hidden' id='ledHue' value='240'>");
  html += F("</div>");

  html += F("<div style='display:flex;gap:8px;align-items:end;'>");

  html += F("<div style='flex:1;min-width:0;'><label>R</label><input type='number' id='ledR' name='r' min='0' max='255' value='");
  html += String(ledR);
  html += F("'></div>");

  html += F("<div style='flex:1;min-width:0;'><label>G</label><input type='number' id='ledG' name='g' min='0' max='255' value='");
  html += String(ledG);
  html += F("'></div>");

  html += F("<div style='flex:1;min-width:0;'><label>B</label><input type='number' id='ledB' name='b' min='0' max='255' value='");
  html += String(ledB);
  html += F("'></div>");

  html += F("</div>");

  html += F("<div style='flex:1;min-width:0;'><label>");
  html += tr(lang, "Jas", "Brightness");
  html += F("</label>");
  html += F("<input type='range' name='brightness' min='0' max='255' value='");
  html += String(ledBrightness);
  html += F("'></div>");

  html += F("<div style='display:flex;gap:10px;align-items:center;margin-top:12px;'><div id='ledPreview' style='width:32px;height:32px;border-radius:8px;border:1px solid #ccc;background:");
  html += colorToHex(ledR, ledG, ledB);
  html += F("'></div><div><b>");
  html += tr(lang, "Aktuální barva:", "Current color:");
  html += F("</b> R=<span id='ledTextR'>");
  html += String(ledR);
  html += F("</span> G=<span id='ledTextG'>");
  html += String(ledG);
  html += F("</span> B=<span id='ledTextB'>");
  html += String(ledB);
  html += F("</span></div></div>");

  html += F("<button type='submit'>");
  html += tr(lang, "Nastavit LED", "Set LED");
  html += F("</button>");
  html += F("</form>");

  html += F("<form action='/led' method='get'>"
            "<input type='hidden' name='lang' value='");
  html += langCode(lang);
  html += F("'>"
            "<input type='hidden' name='r' value='0'>"
            "<input type='hidden' name='g' value='0'>"
            "<input type='hidden' name='b' value='0'>"
            "<button type='submit'>");
  html += tr(lang, "Vypnout LED", "Turn off LED");
  html += F("</button>"
            "</form>");

  return html;
}

String buildRS485Panel(UiLang lang) {
  String html;
  html.reserve(3600);

  html += F("<h2>RS485</h2>"
            "<form action='/rs485' method='post'>"
            "<input type='hidden' name='lang' value='");
  html += langCode(lang);
  html += F("'>"
            "<label>");
  html += tr(lang, "Text k odeslání", "Text to send");
  html += F("</label>"
            "<textarea name='msg' rows='5'></textarea>");

  html += F("<div style='display:flex;gap:10px;align-items:end;flex-wrap:wrap;'>");

  html += F("<div style='min-width:180px;'><label>");
  html += tr(lang, "Přenosová rychlost", "Baud rate");
  html += F("</label><select name='baud'>");

  const uint32_t baudList[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
  for (size_t i = 0; i < sizeof(baudList) / sizeof(baudList[0]); i++) {
    html += F("<option value='");
    html += String(baudList[i]);
    html += F("'");
    if (baudList[i] == rs485Baudrate) html += F(" selected");
    html += F(">");
    html += String(baudList[i]);
    html += F("</option>");
  }

  html += F("</select></div>");

  html += F("<div><button type='submit'>");
  html += tr(lang, "Poslat přes RS485", "Send via RS485");
  html += F("</button></div></div>"
            "</form>"
            "<p><b>");
  html += tr(lang, "Poslední odesláno:", "Last sent:");
  html += F("</b></p><div class='mono' id='rs485LastSent'>");
  html += htmlEscape(rs485LastSent);
  html += F("</div><p><b>");
  html += tr(lang, "Poslední přijato:", "Last received:");
  html += F("</b></p><div class='mono' id='rs485LastReceived'>");
  html += htmlEscape(rs485LastReceived);
  html += F("</div>");

  return html;
}

String buildRS485RxJson() {
  String json = "{";
  json += "\"version\":";
  json += String(rs485RxVersion);
  json += ",\"received\":\"";
  json += jsonEscape(rs485LastReceived);
  json += "\"}";
  return json;
}

String buildLogPanel(UiLang lang) {
  String html;
  html.reserve(12000);

  html += F("<h2>");
  html += tr(lang, "Log", "Log");
  html += F("</h2><div class='mono' id='logContent'>");

  for (size_t i = 0; i < eventLog.size(); i++) {
    html += htmlEscape(renderLogEntry(eventLog[i], lang));
    if (i + 1 < eventLog.size()) html += '\n';
  }

  html += F("</div>");
  return html;
}

String buildMainPage(const String& currentPath, UiLang lang) {
  String html;
  html.reserve(52000);

  html += F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESPLan v2.x Control Panel</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f4f7fb;color:#222;margin:0;padding:20px;}"
    ".wrap{max-width:1400px;margin:0 auto;}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:16px;}"
    ".card{background:#fff;border-radius:14px;padding:16px;box-shadow:0 2px 12px rgba(0,0,0,.08);}"
    ".card-wide{grid-column:1 / -1;}"
    ".tools{display:grid;grid-template-columns:2fr 1fr;gap:16px;align-items:start;margin-bottom:16px;}"
    ".toolbox{background:#f8fafc;border:1px solid #dbe3ee;border-radius:12px;padding:12px;}"
    "h1,h2,h3{margin-top:0}"
    ".page-title{display:flex;align-items:center;gap:18px;flex-wrap:wrap}"
    ".page-title h1{margin:0}"
    ".brand-logo{display:block;max-width:180px;width:100%;height:auto}"
    "label{display:block;margin:8px 0 4px;font-weight:bold}"
    "input[type=text],input[type=number],input[type=file],textarea,select{width:100%;padding:10px;border:1px solid #ccc;border-radius:8px;box-sizing:border-box}"
    "button{padding:10px 14px;border:0;border-radius:10px;background:#1f6feb;color:#fff;cursor:pointer;margin-top:8px}"
    "button:hover{opacity:.92}"
    "table{width:100%;border-collapse:collapse}"
    "th,td{padding:8px;border-bottom:1px solid #ddd;text-align:left;font-size:14px;vertical-align:top}"
    ".mono{font-family:monospace;white-space:pre-wrap;background:#eef2f7;padding:10px;border-radius:10px;max-height:280px;overflow:auto}"
    ".ok{color:#0a7a2f;font-weight:bold}"
    ".bad{color:#b00020;font-weight:bold}"
    ".warn{color:#b26a00;font-weight:bold}"
    "a{color:#1f6feb;text-decoration:none}"
    "a:hover{text-decoration:underline}"
    "@media (max-width:900px){.tools{grid-template-columns:1fr;}.brand-logo{max-width:240px;}}"
    "</style></head><body><div class='wrap'>"
  );

  html += F("<div style='display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:22px;'>");

  html += F("<div class='page-title'>"
            "<img src='/logo.png' alt='LaskaKit logo' class='brand-logo'>"
            "<h1>ESPLan v2.x Control Panel</h1>"
            "</div>");

  html += F("<form method='GET' action='/' style='display:flex;gap:8px;align-items:center;margin:0;'>");
  html += F("<input type='hidden' name='path' value='");
  html += htmlEscape(currentPath);
  html += F("'>");
  html += F("<label style='margin:0;font-weight:bold;'>");
  html += tr(lang, "Jazyk", "Language");
  html += F("</label>");
  html += F("<select name='lang' onchange='this.form.submit()' style='padding:8px;border-radius:8px;border:1px solid #ccc;'>");
  html += F("<option value='cz'");
  if (lang == LANG_CZ) html += F(" selected");
  html += F(">CZ</option>");
  html += F("<option value='en'");
  if (lang == LANG_EN) html += F(" selected");
  html += F(">ENG</option>");
  html += F("</select></form>");

  html += F("</div>");

  html += F("<div class='grid'>");

  html += F("<div class='card' id='statusPanel'>");
  html += buildStatusPanel(lang);
  html += F("</div>");

  html += F("<div class='card' id='neoPixelPanel'>");
  html += buildNeoPixelPanel(lang);
  html += F("</div>");

  html += F("<div class='card' id='rs485Panel'>");
  html += buildRS485Panel(lang);
  html += F("</div>");

  html += F("<div class='card card-wide' id='sdPanel'>");
  html += buildSdPanel(currentPath, lang);
  html += F("</div>");

  html += F("<div class='card' id='logPanel'>");
  html += buildLogPanel(lang);
  html += F("</div>");

  html += F("</div>");

  html += F("<script>");
  html += F("const currentLang='");
  html += langCode(lang);
  html += F("';");

  html += F(
    "let lastSdVersion=0;"

    "function scrollLogToBottom(){"
      "const log=document.getElementById('logContent');"
      "if(log){log.scrollTop=log.scrollHeight;}"
    "}"

    "function clamp(v,min,max){"
      "v=parseFloat(v);"
      "if(isNaN(v)) v=min;"
      "if(v<min) v=min;"
      "if(v>max) v=max;"
      "return v;"
    "}"

    "function hsvToRgb(h,s,v){"
      "let c=v*s;"
      "let x=c*(1-Math.abs((h/60)%2-1));"
      "let m=v-c;"
      "let r=0,g=0,b=0;"
      "if(h>=0&&h<60){r=c;g=x;b=0;}"
      "else if(h<120){r=x;g=c;b=0;}"
      "else if(h<180){r=0;g=c;b=x;}"
      "else if(h<240){r=0;g=x;b=c;}"
      "else if(h<300){r=x;g=0;b=c;}"
      "else {r=c;g=0;b=x;}"
      "return {r:Math.round((r+m)*255),g:Math.round((g+m)*255),b:Math.round((b+m)*255)};"
    "}"

    "function rgbToHsv(r,g,b){"
      "r=r/255; g=g/255; b=b/255;"
      "let max=Math.max(r,g,b), min=Math.min(r,g,b);"
      "let d=max-min;"
      "let h=0, s=max===0?0:d/max, v=max;"
      "if(d!==0){"
        "switch(max){"
          "case r: h=60*(((g-b)/d)%6); break;"
          "case g: h=60*(((b-r)/d)+2); break;"
          "case b: h=60*(((r-g)/d)+4); break;"
        "}"
      "}"
      "if(h<0) h+=360;"
      "return {h:h,s:s,v:v};"
    "}"

    "function bindNeoPicker(){"
      "const area=document.getElementById('ledColorArea');"
      "const cursor=document.getElementById('ledColorCursor');"
      "const hueWrap=document.getElementById('ledHueWrap');"
      "const hueCursor=document.getElementById('ledHueCursor');"
      "const hueInput=document.getElementById('ledHue');"
      "const r=document.getElementById('ledR');"
      "const g=document.getElementById('ledG');"
      "const b=document.getElementById('ledB');"
      "const preview=document.getElementById('ledPreview');"
      "const textR=document.getElementById('ledTextR');"
      "const textG=document.getElementById('ledTextG');"
      "const textB=document.getElementById('ledTextB');"

      "if(!area||!cursor||!hueWrap||!hueCursor||!hueInput||!r||!g||!b) return;"
      "if(area.dataset.bound==='1') return;"
      "area.dataset.bound='1';"

      "let hsv=rgbToHsv(clamp(r.value,0,255),clamp(g.value,0,255),clamp(b.value,0,255));"

      "function updateTexts(rr,gg,bb){"
        "if(preview) preview.style.background='rgb('+rr+','+gg+','+bb+')';"
        "if(textR) textR.textContent=rr;"
        "if(textG) textG.textContent=gg;"
        "if(textB) textB.textContent=bb;"
      "}"

      "function updateAreaBackground(){"
        "const pure=hsvToRgb(hsv.h,1,1);"
        "area.style.background='rgb('+pure.r+','+pure.g+','+pure.b+')';"
      "}"

      "function updateCursors(){"
        "const rect=area.getBoundingClientRect();"
        "const x=hsv.s*rect.width;"
        "const y=(1-hsv.v)*rect.height;"
        "cursor.style.left=x+'px';"
        "cursor.style.top=y+'px';"
        "const hueRect=hueWrap.getBoundingClientRect();"
        "hueCursor.style.left=(hsv.h/360*hueRect.width)+'px';"
      "}"

      "function updateRgbFromHsv(){"
        "const rgb=hsvToRgb(hsv.h,hsv.s,hsv.v);"
        "r.value=rgb.r;"
        "g.value=rgb.g;"
        "b.value=rgb.b;"
        "hueInput.value=Math.round(hsv.h);"
        "updateAreaBackground();"
        "updateTexts(rgb.r,rgb.g,rgb.b);"
        "updateCursors();"
      "}"

      "function updateHsvFromRgb(){"
        "hsv=rgbToHsv(clamp(r.value,0,255),clamp(g.value,0,255),clamp(b.value,0,255));"
        "hueInput.value=Math.round(hsv.h);"
        "updateAreaBackground();"
        "updateTexts(clamp(r.value,0,255),clamp(g.value,0,255),clamp(b.value,0,255));"
        "updateCursors();"
      "}"

      "function pickArea(clientX,clientY){"
        "const rect=area.getBoundingClientRect();"
        "const x=clamp(clientX-rect.left,0,rect.width);"
        "const y=clamp(clientY-rect.top,0,rect.height);"
        "hsv.s=rect.width<=0?0:x/rect.width;"
        "hsv.v=rect.height<=0?0:1-(y/rect.height);"
        "updateRgbFromHsv();"
      "}"

      "function pickHue(clientX){"
        "const rect=hueWrap.getBoundingClientRect();"
        "const x=clamp(clientX-rect.left,0,rect.width);"
        "hsv.h=rect.width<=0?0:(x/rect.width)*360;"
        "if(hsv.h>=360) hsv.h=359.999;"
        "updateRgbFromHsv();"
      "}"

      "let draggingArea=false;"
      "let draggingHue=false;"

      "area.addEventListener('mousedown',function(e){draggingArea=true;pickArea(e.clientX,e.clientY);});"
      "hueWrap.addEventListener('mousedown',function(e){draggingHue=true;pickHue(e.clientX);});"

      "window.addEventListener('mousemove',function(e){"
        "if(draggingArea) pickArea(e.clientX,e.clientY);"
        "if(draggingHue) pickHue(e.clientX);"
      "});"

      "window.addEventListener('mouseup',function(){draggingArea=false;draggingHue=false;});"

      "area.addEventListener('touchstart',function(e){"
        "e.preventDefault();"
        "draggingArea=true;"
        "const t=e.touches[0];"
        "pickArea(t.clientX,t.clientY);"
      "},{passive:false});"

      "hueWrap.addEventListener('touchstart',function(e){"
        "e.preventDefault();"
        "draggingHue=true;"
        "const t=e.touches[0];"
        "pickHue(t.clientX);"
      "},{passive:false});"

      "window.addEventListener('touchmove',function(e){"
        "if(draggingArea&&e.touches.length){"
          "e.preventDefault();"
          "const t=e.touches[0];"
          "pickArea(t.clientX,t.clientY);"
        "}"
        "if(draggingHue&&e.touches.length){"
          "e.preventDefault();"
          "const t=e.touches[0];"
          "pickHue(t.clientX);"
        "}"
      "},{passive:false});"

      "window.addEventListener('touchend',function(){draggingArea=false;draggingHue=false;});"

      "r.addEventListener('input',updateHsvFromRgb);"
      "g.addEventListener('input',updateHsvFromRgb);"
      "b.addEventListener('input',updateHsvFromRgb);"

      "updateHsvFromRgb();"
    "}"

    "async function refreshPanel(url,id,after){"
      "try{"
        "const r=await fetch(url,{cache:'no-store'});"
        "if(!r.ok)return;"
        "const t=await r.text();"
        "const el=document.getElementById(id);"
        "if(el){"
          "el.innerHTML=t;"
          "if(after){after();}"
        "}"
      "}catch(e){}"
    "}"

    "async function pollSdState(){"
      "try{"
        "const r=await fetch('/sdstate?lang='+currentLang,{cache:'no-store'});"
        "if(!r.ok)return;"
        "const s=await r.json();"
        "if(lastSdVersion===0){"
          "lastSdVersion=s.version;"
          "return;"
        "}"
        "if(s.version!==lastSdVersion){"
          "lastSdVersion=s.version;"
          "const p=new URLSearchParams(window.location.search).get('path')||'/';"
          "refreshPanel('/sdpanel?path='+encodeURIComponent(p)+'&lang='+currentLang,'sdPanel',null);"
          "refreshPanel('/status?lang='+currentLang,'statusPanel',null);"
        "}"
      "}catch(e){}"
    "}"

    "let lastRs485RxVersion=0;"

    "async function pollRs485Rx(){"
      "try{"
        "const r=await fetch('/rs485rx?lang='+currentLang,{cache:'no-store'});"
        "if(!r.ok)return;"
        "const s=await r.json();"
        "const el=document.getElementById('rs485LastReceived');"
        "if(lastRs485RxVersion===0){"
          "lastRs485RxVersion=s.version;"
          "if(el){el.textContent=(s.received||'');}"
          "return;"
        "}"
        "if(s.version!==lastRs485RxVersion){"
          "lastRs485RxVersion=s.version;"
          "if(el){el.textContent=(s.received||'');}"
        "}"
      "}catch(e){}"
    "}"

    "setInterval(function(){refreshPanel('/status?lang='+currentLang,'statusPanel',null);},1000);"
    "setInterval(function(){refreshPanel('/logpanel?lang='+currentLang,'logPanel',scrollLogToBottom);},2000);"
    "setInterval(pollSdState,1000);"
    "setInterval(pollRs485Rx,500);"

    "window.addEventListener('load',function(){"
      "scrollLogToBottom();"
      "bindNeoPicker();"
      "pollSdState();"
      "pollRs485Rx();"
    "});"
  );

  html += F("</script>");
  html += F("</div></body></html>");
  return html;
}

// =====================================================
// HTTP HANDLERY
// =====================================================

void handleRoot() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;
  String path = normalizeDirPath(server.arg("path"));
  if (!isSafePath(path)) path = "/";
  server.send(200, "text/html; charset=utf-8", buildMainPage(path, lang));
}

void handleStatus() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;
  server.send(200, "text/html; charset=utf-8", buildStatusPanel(lang));
}

void handleRS485Panel() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;
  server.send(200, "text/html; charset=utf-8", buildRS485Panel(lang));
}

void handleRS485Rx() {
  updateCurrentLangFromRequest();
  server.send(200, "application/json; charset=utf-8", buildRS485RxJson());
}

void handleLogPanel() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;
  server.send(200, "text/html; charset=utf-8", buildLogPanel(lang));
}

void handleSdPanel() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;
  String path = normalizeDirPath(server.arg("path"));
  if (!isSafePath(path)) path = "/";
  server.send(200, "text/html; charset=utf-8", buildSdPanel(path, lang));
}

void handleSdState() {
  updateCurrentLangFromRequest();
  server.send(200, "application/json; charset=utf-8", buildSdStateJson());
}

void handleLed() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;

  int r = constrain(server.arg("r").toInt(), 0, 255);
  int g = constrain(server.arg("g").toInt(), 0, 255);
  int b = constrain(server.arg("b").toInt(), 0, 255);

  int brightness = constrain(server.arg("brightness").toInt(), 0, 255);
  ledBrightness = brightness;

  setPixelColor(r, g, b);
  addLogEntry(LOG_LED_SET, String(r), String(g), String(b));
  redirectHome(lang);
}

void handleRS485Send() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;

  uint32_t requestedBaud = (uint32_t)server.arg("baud").toInt();
  if (isSupportedRs485Baud(requestedBaud) && requestedBaud != rs485Baudrate) {
    applyRS485Baudrate(requestedBaud);
  }

  String msg = server.arg("msg");
  msg.replace("\r", "");

  if (msg.length() == 0) {
    redirectHome(lang);
    return;
  }

  Serial2.println(msg);
  Serial2.flush();

  rs485LastSent = msg;
  addLogEntry(LOG_RS485_TX, msg);
  redirectHome(lang);
}

void handleDownload() {
  updateCurrentLangFromRequest();

  if (!sdOk) {
    server.send(500, "text/plain", trCurrent("SD karta není dostupná", "SD card is not available"));
    return;
  }

  String name = normalizePath(server.arg("name"));

  if (!isSafePath(name)) {
    server.send(400, "text/plain", trCurrent("Neplatná cesta", "Invalid path"));
    return;
  }

  if (!SD.exists(name)) {
    server.send(404, "text/plain", trCurrent("Soubor neexistuje", "File does not exist"));
    return;
  }

  File file = SD.open(name, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", trCurrent("Nelze otevřít soubor", "Cannot open file"));
    return;
  }

  if (file.isDirectory()) {
    file.close();
    server.send(400, "text/plain", trCurrent("Adresář nelze stáhnout", "Directory cannot be downloaded"));
    return;
  }

  String downloadName = String(file.name());
  downloadName = normalizePath(downloadName);
  if (downloadName.startsWith("/")) {
    downloadName = downloadName.substring(1);
  }

  server.sendHeader("Content-Disposition", "attachment; filename=\"" + downloadName + "\"");
  server.streamFile(file, contentTypeFromFilename(name));
  file.close();
}

void handleDelete() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;

  if (!sdOk) {
    server.send(500, "text/plain", trCurrent("SD karta není dostupná", "SD card is not available"));
    return;
  }

  String name = normalizePath(server.arg("name"));
  String currentPath = normalizeDirPath(server.arg("path"));

  if (!isSafePath(name) || !isSafePath(currentPath)) {
    server.send(400, "text/plain", trCurrent("Neplatná cesta", "Invalid path"));
    return;
  }

  if (!SD.exists(name)) {
    server.send(404, "text/plain", trCurrent("Soubor nebo adresář neexistuje", "File or directory does not exist"));
    return;
  }

  File entry = SD.open(name);
  if (!entry) {
    server.send(500, "text/plain", trCurrent("Nelze otevřít položku", "Cannot open item"));
    return;
  }

  bool isDir = entry.isDirectory();
  entry.close();

  bool ok = false;
  if (isDir) {
    ok = deleteRecursive(name);
  } else {
    ok = SD.remove(name);
  }

  if (ok) {
    addLogEntry(LOG_SD_DELETE_OK, name);
  } else {
    addLogEntry(LOG_SD_DELETE_ERROR, name);
  }

  bumpSdStateVersion();
  redirectToPath(currentPath, lang);
}

void handleMkdir() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;

  if (!sdOk) {
    server.send(500, "text/plain", trCurrent("SD karta není dostupná", "SD card is not available"));
    return;
  }

  String currentPath = normalizeDirPath(server.arg("path"));
  String name = server.arg("name");
  name.trim();

  if (!isSafePath(currentPath)) currentPath = "/";

  if (name.length() == 0) {
    redirectToPath(currentPath, lang);
    return;
  }

  name.replace("\\", "/");
  while (name.startsWith("/")) name.remove(0, 1);
  while (name.endsWith("/")) name.remove(name.length() - 1);

  if (name.length() == 0 || name.indexOf("..") >= 0 || name.indexOf('/') >= 0) {
    addLogEntry(LOG_MKDIR_INVALID);
    redirectToPath(currentPath, lang);
    return;
  }

  String newDir = joinPath(currentPath, name);

  if (SD.exists(newDir)) {
    addLogEntry(LOG_MKDIR_EXISTS, newDir);
    redirectToPath(currentPath, lang);
    return;
  }

  if (SD.mkdir(newDir)) {
    addLogEntry(LOG_MKDIR_OK, newDir);
  } else {
    addLogEntry(LOG_MKDIR_ERROR, newDir);
  }

  bumpSdStateVersion();
  redirectToPath(currentPath, lang);
}

void handleUploadFinished() {
  updateCurrentLangFromRequest();
  UiLang lang = currentUiLang;
  String path = normalizeDirPath(server.arg("path"));
  if (!isSafePath(path)) path = "/";
  bumpSdStateVersion();
  redirectToPath(path, lang);
}

void handleFileUpload() {
  if (!sdOk) return;

  HTTPUpload& upload = server.upload();
  static String uploadTargetPath = "/";

  if (upload.status == UPLOAD_FILE_START) {
    updateCurrentLangFromRequest();

    uploadTargetPath = normalizeDirPath(server.arg("path"));
    if (!isSafePath(uploadTargetPath)) uploadTargetPath = "/";

    String cleanName = upload.filename;
    cleanName.replace("\\", "/");
    int slashPos = cleanName.lastIndexOf('/');
    if (slashPos >= 0) cleanName = cleanName.substring(slashPos + 1);

    String filename = joinPath(uploadTargetPath, cleanName);

    if (!isSafePath(filename)) {
      addLogEntry(LOG_UPLOAD_INVALID_NAME);
      return;
    }

    addLogEntry(LOG_UPLOAD_START, filename);

    if (SD.exists(filename)) {
      SD.remove(filename);
    }

    uploadFile = SD.open(filename, FILE_WRITE);
    if (!uploadFile) {
      addLogEntry(LOG_UPLOAD_OPEN_WRITE_FAILED);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
      yield();
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      addLogEntry(LOG_UPLOAD_FINISHED, String(upload.totalSize));
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) {
      uploadFile.close();
    }
    addLogEntry(LOG_UPLOAD_ABORTED);
  }
}

void handleLogo() {
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.send_P(200, "image/png", (PGM_P)logo_web_png, logo_web_png_len);
}

void handleNotFound() {
  updateCurrentLangFromRequest();
  server.send(404, "text/plain", trCurrent("404 Nenalezeno", "404 Not Found"));
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/rs485panel", HTTP_GET, handleRS485Panel);
  server.on("/rs485rx", HTTP_GET, handleRS485Rx);
  server.on("/logpanel", HTTP_GET, handleLogPanel);
  server.on("/sdpanel", HTTP_GET, handleSdPanel);
  server.on("/sdstate", HTTP_GET, handleSdState);
  server.on("/logo.png", HTTP_GET, handleLogo);
  server.on("/led", HTTP_GET, handleLed);
  server.on("/rs485", HTTP_POST, handleRS485Send);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/mkdir", HTTP_GET, handleMkdir);
  server.on("/upload", HTTP_POST, handleUploadFinished, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();
  addLogEntry(LOG_WEB_STARTED);
}

// =====================================================
// SETUP / LOOP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  printResetReason();
  delay(500);

  addLogText("");
  addLogText("======================================");
  addLogEntry(LOG_START);
  addLogText("======================================");

  pixel.begin();
  pixel.clear();
  pixel.show();
  pixel.setBrightness(ledBrightness);
  setPixelColor(0, 255, 0);

  totalHeapBytes = ESP.getHeapSize();

  addLogEntry(LOG_CPU_FREQ, String(ESP.getCpuFreqMHz()));
  addLogEntry(LOG_HEAP_TOTAL, String(totalHeapBytes));
  addLogEntry(LOG_HEAP_FREE, String(ESP.getFreeHeap()), String(getFreeHeapPercent(), 1));

  initI2CDevices();

  lastSdDetectRaw = isSdCardPresent();
  lastSdDetectStable = lastSdDetectRaw;
  lastSdDetectChangeMs = millis();

  initSDCard();
  initRS485();
  initEthernet();
  setupWebServer();

  addLogEntry(LOG_READY);
  if (ethGotIP) {
    addLogEntry(LOG_OPEN_BROWSER, ETH.localIP().toString());
  } else {
    addLogEntry(LOG_ETH_NO_IP_YET);
  }
}

void loop() {
  server.handleClient();

  handleSdHotplug();

  bool rs485RxChanged = false;
  static String rs485LineBuffer = "";

  while (Serial2.available()) {
    char c = (char)Serial2.read();
    Serial.write(c);

    rs485LastReceived += c;
    rs485LineBuffer += c;
    rs485RxChanged = true;

    if (rs485LastReceived.length() > 2000) {
      rs485LastReceived.remove(0, rs485LastReceived.length() - 2000);
    }

    if (c == '\n') {
      String line = rs485LineBuffer;
      line.trim();
      if (line.length() > 0) {
        addLogEntry(LOG_RS485_RX, line);
      }
      rs485LineBuffer = "";
    }

    if ((rs485LastReceived.length() & 0xFF) == 0) {
      yield();
    }
  }

  if (rs485RxChanged) {
    rs485RxVersion++;
  }

  if (millis() - lastSensorRead > 10000) {
    lastSensorRead = millis();

    if (sht40Found) {
      readSHT40();
    }

    if (bmp280Found) {
      readBMP280();
    }
  }

  if (millis() - lastStatusPrint > 5000) {
    lastStatusPrint = millis();

    Serial.println();
    Serial.println(F("----- STATUS -----"));
    Serial.printf("ETH link: %s\n", ethConnected ? "UP" : "DOWN");
    Serial.printf("ETH IP: %s\n", ethGotIP ? ETH.localIP().toString().c_str() : "none");
    Serial.printf("SD: %s\n", sdOk ? "OK" : "ERROR");
    Serial.printf("SD detect: %s\n", sdCardInserted ? "inserted" : "not inserted");
    Serial.printf("SD version: %lu\n", (unsigned long)sdStateVersion);
    Serial.printf("SHT40: %s\n", sht40Found ? "OK" : "not found");
    if (sht40Found && !isnan(lastTemperature) && !isnan(lastHumidity)) {
      Serial.printf("SHT40 -> T=%.2f C, RH=%.2f %%\n", lastTemperature, lastHumidity);
    }

    Serial.printf("BMP280: %s\n", bmp280Found ? "OK" : "not found");
    if (bmp280Found && !isnan(lastBmpTemperature) && !isnan(lastPressure_hPa)) {
      Serial.printf("BMP280 -> T=%.2f C, P=%.2f hPa\n", lastBmpTemperature, lastPressure_hPa);
    }

    Serial.printf("RS485 baud: %lu\n", (unsigned long)rs485Baudrate);
    Serial.printf("LED -> R=%u G=%u B=%u\n", ledR, ledG, ledB);
    Serial.printf("FreeHeap: %u B (%.1f %%)\n", ESP.getFreeHeap(), getFreeHeapPercent());
    Serial.printf("Heap total: %u B\n", totalHeapBytes);
    Serial.println(F("------------------"));
  }

  delay(1);
}