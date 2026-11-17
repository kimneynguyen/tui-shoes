#pragma once
#include <stdint.h>

// What one shoe sends, 25 times a second. 20 bytes, little endian.
// The same bytes go Minima -> XIAO (SDA/SCL) -> laptop (Bluetooth).
// Must match minima_foot/packet.h and sampler/index.html.
struct __attribute__((packed)) FootPacket {
  uint8_t  state;     // 0 still, 1 walk, 2 run, 3 spin
  uint8_t  spins;     // goes up by 1 after every full spin (wraps 255 -> 0)
  uint8_t  flags;     // bit 0 gyro found, bit 1 foot down, bit 2 XIAO answering
  uint8_t  spinPct;   // how far into a spin right now, 0..100
  uint16_t fsr;       // pressure, 0..4095
  uint16_t steps;     // goes up by 1 every step of this foot
  uint16_t stepMs;    // time between the last two steps of this foot (0 = none)
  int16_t  ax, ay, az;// accelerometer, in 1/100 g
  int16_t  yawRate;   // turning speed around the up/down line, deg/s
  uint16_t gyro;      // how fast the foot is rotating in any direction, deg/s
};

static_assert(sizeof(FootPacket) == 20, "FootPacket must be 20 bytes (fits one Bluetooth message)");

#define FLAG_IMU_OK   0x01
#define FLAG_FOOT_DOWN 0x02
#define FLAG_XIAO_OK  0x04
