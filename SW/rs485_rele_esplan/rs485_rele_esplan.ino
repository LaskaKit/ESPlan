//  ESPLan + https://www.laskakit.cz/8-kanalu-12v-rele-modul-250vac-10a--rs485--din/

byte open1ch[] = {0x01, 0x06, 0x00, 0x01, 0x01, 0x00};
byte close1ch[] = {0x01, 0x06, 0x00, 0x01, 0x02, 0x00};

byte openAll[] = {0x01, 0x06, 0x00, 0x00, 0x07, 0x00};
byte closeAll[] = {0x01, 0x06, 0x00, 0x00, 0x08, 0x00};


void setup(){
  Serial.begin(115200);

  Serial1.begin(9600, SERIAL_8N1, 36, 4); //ESP32 RX1 IO36, TX1 IO4

//  sendData(openAll, sizeof(openAll));  //  Sepne vsechny rele
//  delay(1000);
//  sendData(closeAll, sizeof(closeAll));  //  Rozepne vsechny rele
//  delay(1000);
  sendData(open1ch, sizeof(open1ch));  //  Sepne rele 1
  delay(1000);
  sendData(close1ch, sizeof(close1ch));  //  Rozepne rele 1
}

void loop(){

}

void sendData(byte d[], int i){
  byte crc[2];

  uint16_t crcc = calcCRC(d, i);
  uint8_t crc_l = (uint8_t)(crcc >> 8);
  uint8_t crc_h = (uint8_t)crcc;
  d[6] = crc_h;
  d[7] = crc_l;

  Serial1.write(d, i+2);
}

unsigned short calcCRC(const unsigned char *indata, int len){
  unsigned short crc=0xffff;
  unsigned char temp;
  int n;
  while(len--){
    crc=*indata^crc;
    for(n=0;n<8;n++){
      char TT;
      TT=crc&1;
      crc=crc>>1;
      crc=crc&0x7fff;
      if (TT==1){
        crc=crc^0xa001;
        crc=crc&0xffff;
      }
    }
    indata++;
  }
  return crc;
}
