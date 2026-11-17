#pragma once
// Sends each FootPacket to the XIAO over the same SDA/SCL wires the gyro uses.
// Wiring: XIAO 5V -> 5V, GND -> GND, D4 -> SCL, D5 -> SDA. No resistors.
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "packet.h"

class XiaoLink {
public:
  uint8_t lastErr = 0xFF;     // 0 = the XIAO got it
  uint32_t sent = 0, failed = 0;

  bool send(const FootPacket &p) {
    Wire.beginTransmission(XIAO_ADDR);
    Wire.write((const uint8_t *)&p, sizeof(p));
    lastErr = Wire.endTransmission();
    if (lastErr == 0) sent++; else failed++;
    return lastErr == 0;
  }

  bool ok() const { return lastErr == 0; }
};
