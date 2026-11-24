/*
  SDA/SCL wire check (XIAO ESP32-C6, board XIAO_ESP32C6)
  Wiring: gyro SDA -> D4, SCL -> D5 (and the TCS34725 on the same two wires later).

  Open Serial Monitor at 115200. Every 2 seconds it lists who answers:
     0x68  the gyro board (0x69 if its AD0 pin goes to VCC)
     0x29  the TCS34725 color sensor
  Nothing found: check SDA/SCL and that the + rail has 3.3 V.
*/
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(D4, D5);   // SDA, SCL
  Wire.setClock(100000);
}

void loop() {
  Serial.print("# found:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(" 0x"); if (a < 16) Serial.print('0'); Serial.print(a, HEX);
      if (a == 0x68 || a == 0x69) Serial.print(" (gyro)");
      if (a == 0x29) Serial.print(" (color sensor)");
      n++;
    }
  }
  Serial.println(n ? "" : " nothing. Check the SDA/SCL wires and power.");
  delay(2000);
}
