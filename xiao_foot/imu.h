#pragma once
// MPU-6050 gyro + accelerometer (GY-521 board), read with plain Wire calls.
// Wiring: VCC -> + rail (3.3 V), GND -> - rail, SDA -> XIAO D4, SCL -> XIAO D5.
// Wire.begin(SDA_PIN, SCL_PIN) is called in xiao_foot.ino.
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class Mpu6050 {
public:
  bool ok = false;
  float ax = 0, ay = 0, az = 0;   // g
  float gx = 0, gy = 0, gz = 0;   // deg/s, with the resting offset removed
  float biasX = 0, biasY = 0, biasZ = 0;

  bool begin() {
    write(0x6B, 0x00);   // wake up
    delay(50);
    write(0x1A, 0x03);   // low pass filter ~44 Hz
    write(0x1B, 0x18);   // gyro +/-2000 deg/s (a fast spin can pass 1000)
    write(0x1C, 0x10);   // accel +/-8 g (running hits are big)
    Wire.beginTransmission(MPU_ADDR);
    ok = (Wire.endTransmission() == 0);
    return ok;
  }

  bool read() {
    int16_t v[7];
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) { ok = false; return false; }
    if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14) != 14) { ok = false; return false; }
    for (int i = 0; i < 7; i++) {
      uint8_t hi = Wire.read(), lo = Wire.read();
      v[i] = (int16_t)((hi << 8) | lo);
    }
    ax = v[0] / 4096.0f; ay = v[1] / 4096.0f; az = v[2] / 4096.0f;
    gx = v[4] / 16.4f - biasX; gy = v[5] / 16.4f - biasY; gz = v[6] / 16.4f - biasZ;
    ok = true;
    return true;
  }

  // Measure the gyro's resting offset, and which way is up while it rests
  // (restAx/Ay/Az, used as "flat"). The shoe must be still.
  float restAx = 0, restAy = 0, restAz = 1;
  void calibrate(uint16_t samples = 200) {
    biasX = biasY = biasZ = 0;
    float sx = 0, sy = 0, sz = 0, sax = 0, say = 0, saz = 0;
    uint16_t n = 0;
    for (uint16_t i = 0; i < samples; i++) {
      if (read()) { sx += gx; sy += gy; sz += gz; sax += ax; say += ay; saz += az; n++; }
      delay(5);
    }
    if (n) {
      biasX = sx / n; biasY = sy / n; biasZ = sz / n;
      restAx = sax / n; restAy = say / n; restAz = saz / n;
    }
  }

  float accelG() const { return sqrtf(ax * ax + ay * ay + az * az); }
  float gyroDps() const { return sqrtf(gx * gx + gy * gy + gz * gz); }

private:
  void write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }
};
