#pragma once
// Pressure sensor (FSR) in the sole.
// Wiring: one leg -> 5V, other leg -> A0, resistor from A0 -> GND.
#include <Arduino.h>
#include "config.h"

class FsrSensor {
public:
  uint16_t value = 0;   // 0..4095, higher = more pressure

  void begin() { analogReadResolution(12); }

  uint16_t read() {
    uint32_t sum = 0;
    for (int i = 0; i < 4; i++) sum += analogRead(FSR_PIN);
    value = (uint16_t)(sum / 4);
    return value;
  }
};
