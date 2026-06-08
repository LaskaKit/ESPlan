#include <Arduino.h>

extern "C" {
  #include "esp_efuse.h"
  #include "esp_efuse_table.h"
  #include "esp_system.h"
}

static bool isVddSdioFixedTo3v3(){
  bool force = esp_efuse_read_field_bit(ESP_EFUSE_XPD_SDIO_FORCE);
  bool reg   = esp_efuse_read_field_bit(ESP_EFUSE_XPD_SDIO_REG);
  bool tieh  = esp_efuse_read_field_bit(ESP_EFUSE_XPD_SDIO_TIEH);

  return force && reg && tieh;
}

static void burnVddSdio3v3Once(){
  Serial.println();
  Serial.println("Checking VDD_SDIO eFuse...");

  bool force = esp_efuse_read_field_bit(ESP_EFUSE_XPD_SDIO_FORCE);
  bool reg   = esp_efuse_read_field_bit(ESP_EFUSE_XPD_SDIO_REG);
  bool tieh  = esp_efuse_read_field_bit(ESP_EFUSE_XPD_SDIO_TIEH);

  Serial.printf("XPD_SDIO_FORCE: %d\n", force);
  Serial.printf("XPD_SDIO_REG:   %d\n", reg);
  Serial.printf("XPD_SDIO_TIEH:  %d\n", tieh);

  if (isVddSdioFixedTo3v3()){
    Serial.println("VDD_SDIO already fixed to 3.3V.");
    return;
  }

  Serial.println("Burning eFuse: VDD_SDIO fixed to 3.3V.");
  Serial.println("This is permanent.");

  esp_err_t err;

  err = esp_efuse_write_field_bit(ESP_EFUSE_XPD_SDIO_FORCE);
  if (err != ESP_OK){
    Serial.printf("Failed to burn XPD_SDIO_FORCE: %d\n", err);
    return;
  }

  err = esp_efuse_write_field_bit(ESP_EFUSE_XPD_SDIO_REG);
  if (err != ESP_OK){
    Serial.printf("Failed to burn XPD_SDIO_REG: %d\n", err);
    return;
  }

  err = esp_efuse_write_field_bit(ESP_EFUSE_XPD_SDIO_TIEH);
  if (err != ESP_OK){
    Serial.printf("Failed to burn XPD_SDIO_TIEH: %d\n", err);
    return;
  }

  Serial.println("eFuse burned. Restarting...");
  delay(1000);
  esp_restart();
}

void setup(){
  Serial.begin(115200);
  delay(5000);

  burnVddSdio3v3Once();
}

void loop(){
}