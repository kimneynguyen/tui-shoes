/*
  Gyro board (MPU-6050 / GY-521) test (Minima + gyro only, unplug the XIAO's D4/D5)
  Wiring: VCC -> 5V, GND -> GND, SDA -> SDA (A4), SCL -> SCL (A5).

  Open Serial Monitor or Serial Plotter at 115200.
  Expect, lying flat and still: one of ax/ay/az near 1.00 (or -1.00), the others near 0,
  and gx/gy/gz near 0. Spin it on the table: one of gx/gy/gz jumps to hundreds.
*/
#include <Wire.h>

const uint8_t GYRO_ADDR = 0x68;   // 0x69 if AD0 is wired to VCC

void reg(uint8_t r, uint8_t v) { Wire.beginTransmission(GYRO_ADDR); Wire.write(r); Wire.write(v); Wire.endTransmission(); }

void setup() {
  Serial.begin(115200);
  Wire.begin();
  reg(0x6B, 0x00);   // wake up
  reg(0x1B, 0x18);   // gyro +/-2000 deg/s
  reg(0x1C, 0x10);   // accel +/-8 g
  delay(300);
  Wire.beginTransmission(GYRO_ADDR);
  Wire.write(0x75);  // WHO_AM_I
  Wire.endTransmission(false);
  Wire.requestFrom(GYRO_ADDR, (uint8_t)1);
  int who = Wire.available() ? Wire.read() : -1;
  Serial.print("# WHO_AM_I = 0x"); Serial.print(who, HEX);
  Serial.println(who == 0x68 ? "  (GYRO_ADDR-6050 found)" : (who == -1 ? "  NOT FOUND: check SDA/SCL/VCC/GND" : "  (answered, unusual id but may still work)"));
}

void loop() {
  Wire.beginTransmission(GYRO_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(GYRO_ADDR, (uint8_t)14) != 14) {
    Serial.println("# no answer from the gyro board");
    delay(500);
    return;
  }
  int16_t v[7];
  for (int i = 0; i < 7; i++) { uint8_t h = Wire.read(), l = Wire.read(); v[i] = (int16_t)((h << 8) | l); }
  Serial.print("ax:"); Serial.print(v[0] / 4096.0, 2);
  Serial.print(",ay:"); Serial.print(v[1] / 4096.0, 2);
  Serial.print(",az:"); Serial.print(v[2] / 4096.0, 2);
  Serial.print(",gx:"); Serial.print(v[4] / 16.4, 0);
  Serial.print(",gy:"); Serial.print(v[5] / 16.4, 0);
  Serial.print(",gz:"); Serial.println(v[6] / 16.4, 0);
  delay(40);
}
