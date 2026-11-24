#pragma once
#include <stdint.h>

// What one shoe sends, 25 times a second. 20 bytes, little endian, so it fits
// one Bluetooth notification without asking for a bigger MTU.
// Must match decodePacket() in sampler/index.html.
struct __attribute__((packed)) FootPacket {
  uint8_t  state;     // 0 still, 1 walk, 2 run, 3 spin (this foot only)
  uint8_t  spins;     // goes up by 1 after every full spin (wraps 255 -> 0)
  uint8_t  flags;     // FLAG_* below
  uint8_t  spinPct;   // how far into a spin right now, 0..100
  uint16_t fsr;       // pressure, 0..4095
  uint16_t steps;     // goes up by 1 every time this foot presses down (a step or a landing)
  uint16_t stepMs;    // time between the last two steps of this foot (0 = none)
  int8_t   pitch;     // degrees from flat: + toes up (heel back), - toes down (tiptoe)
  int8_t   roll;      // degrees rolled sideways from flat (clamped to +/-127)
  uint8_t  peakG;     // biggest total acceleration since the last packet, 1/16 g
  uint8_t  gyro;      // how fast the foot is rotating in any direction, 8 deg/s per unit
  int16_t  yawRate;   // turning speed around the up/down line, deg/s
  uint8_t  event;     // last one-foot event (EVENT_* in motion.h)
  uint8_t  events;    // goes up by 1 on every event (wraps 255 -> 0)
  uint8_t  pose;      // POSE_* in motion.h: flat, tiptoe, heel, side, sky
  uint8_t  air10;     // how long the last time in the air lasted (accelerometer, weightless), 10 ms units
};

static_assert(sizeof(FootPacket) == 20, "FootPacket must be 20 bytes (fits one Bluetooth message)");

#define FLAG_IMU_OK    0x01
#define FLAG_FOOT_DOWN 0x02
#define FLAG_XIAO_OK   0x04
#define FLAG_TOUCHING  0x08   // some pressure on the sole (lighter than "down")
#define FLAG_AIRBORNE  0x10   // weightless right now (accelerometer), i.e. in the air
