#pragma once
// Motion logic: steps, spins, and walk / run / still.
// Plain C++ (no Arduino calls), so tests/logic_test can run it on a laptop.
#include <stdint.h>
#include <math.h>
#include "config.h"

enum FootState : uint8_t { STATE_STILL = 0, STATE_WALK = 1, STATE_RUN = 2, STATE_SPIN = 3 };

inline const char *stateName(uint8_t s) {
  switch (s) {
    case STATE_STILL: return "STILL";
    case STATE_WALK:  return "WALK";
    case STATE_RUN:   return "RUN";
    case STATE_SPIN:  return "SPIN";
  }
  return "?";
}

// ---------------------------------------------------------------------
// Steps. Call beginSample() once per sample, then ONE of updateFsr / updateAccel.
// ---------------------------------------------------------------------
class StepDetector {
public:
  uint16_t count = 0;
  uint32_t lastStepMs = 0;
  uint16_t intervalMs = 0;    // time between the last two steps
  uint16_t avgMs = 0;         // average of the last two intervals (0 = not enough steps yet)
  bool down = false;          // foot is pressing
  bool stepped = false;       // true only on the sample a step happened
  float peak = 0;             // learned "full press" level
  float pressAt = FSR_MIN_PRESS, releaseAt = FSR_MIN_PRESS * 0.6f;

  void beginSample() { stepped = false; }

  void updateFsr(uint32_t now, uint16_t fsr) {
    // learn the press level: jump up to new highs, slowly forget old ones (~30 s)
    peak = (fsr > peak) ? fsr : peak * 0.9998f;
    pressAt = fmaxf(FSR_MIN_PRESS, FSR_PRESS_FRAC * peak);
    releaseAt = fmaxf(FSR_MIN_PRESS * 0.6f, FSR_RELEASE_FRAC * peak);
    if (!down && fsr > pressAt) { down = true; registerStep(now); }
    else if (down && fsr < releaseAt) down = false;
  }

  void updateAccel(uint32_t now, float accelG) {
    if (!down && accelG > ACCEL_STEP_G) { down = true; registerStep(now); }
    else if (down && accelG < ACCEL_RESET_G) down = false;
  }

  uint16_t stepsPerMin() const { return avgMs ? (uint16_t)(60000UL / avgMs) : 0; }

private:
  uint16_t prevInterval = 0;

  void registerStep(uint32_t now) {
    if (lastStepMs && now - lastStepMs < STEP_MIN_MS) return;
    uint32_t gap = lastStepMs ? now - lastStepMs : 0;
    if (gap && gap <= STEP_MAX_GAP_MS) {
      intervalMs = (uint16_t)gap;
      avgMs = prevInterval ? (uint16_t)((prevInterval + intervalMs) / 2) : intervalMs;
      prevInterval = intervalMs;
    } else {
      intervalMs = 0; avgMs = 0; prevInterval = 0;   // first step after a pause
    }
    lastStepMs = now;
    count++;
    stepped = true;
  }
};

// ---------------------------------------------------------------------
// Which way is up, so turning can be measured no matter how the board sits.
// ---------------------------------------------------------------------
class GravityTracker {
public:
  float x = 0, y = 0, z = 1;   // slowly follows the accelerometer (in g)

  void update(float ax, float ay, float az) {
    const float a = 0.03f;
    x += a * (ax - x); y += a * (ay - y); z += a * (az - z);
  }
  // turning speed around the up/down line (deg/s). Sign = direction.
  float yawRate(float gx, float gy, float gz) const {
    float n = sqrtf(x * x + y * y + z * z);
    if (n < 0.3f) return 0;
    return (gx * x + gy * y + gz * z) / n;
  }
};

// ---------------------------------------------------------------------
// 360 spin: add up turning while it's fast and in one direction.
// ---------------------------------------------------------------------
class SpinDetector {
public:
  uint8_t spins = 0;          // goes up by 1 after each full spin
  float angle = 0;            // degrees turned in the current attempt
  bool turning = false;       // an attempt is in progress
  bool spun = false;          // true only on the sample a spin finished
  uint32_t lastSpinMs = 0;

  void update(uint32_t now, float yawDps, float dt) {
    spun = false;
    bool fast = fabsf(yawDps) > SPIN_RATE_DPS;
    if (fast) {
      if (turning && angle * yawDps < 0) reset();          // changed direction: start over
      if (!turning) { turning = true; startMs = now; angle = 0; }
      lastFastMs = now;
    }
    if (turning) {
      angle += yawDps * dt;
      if (now - lastFastMs > SPIN_GAP_MS || now - startMs > SPIN_WINDOW_MS) reset();
      else if (fabsf(angle) >= SPIN_DEG) {
        spins++; spun = true; lastSpinMs = now;
        reset();
      }
    }
  }

  uint8_t progressPct() const {
    float p = fabsf(angle) / SPIN_DEG * 100.0f;
    return (uint8_t)(p > 100 ? 100 : p);
  }
  // spinning right now (well into a turn) or just finished one
  bool active(uint32_t now) const {
    return (turning && fabsf(angle) > 90.0f) || (lastSpinMs && now - lastSpinMs < SPIN_SHOW_MS);
  }

private:
  uint32_t startMs = 0, lastFastMs = 0;
  void reset() { turning = false; angle = 0; }
};

// ---------------------------------------------------------------------
// Put it together: STILL / WALK / RUN / SPIN.
// Only steps make WALK or RUN. Tilting or shaking the shoe without
// stepping stays STILL.
// ---------------------------------------------------------------------
class MotionClassifier {
public:
  uint8_t state = STATE_STILL;

  uint8_t update(uint32_t now, const StepDetector &steps, const SpinDetector &spin) {
    bool stepping = steps.lastStepMs && steps.avgMs && now - steps.lastStepMs < STILL_MS;
    if (spin.active(now)) state = STATE_SPIN;
    else if (!stepping) state = STATE_STILL;
    else {
      uint16_t runLimit = RUN_STEP_MS + (state == STATE_RUN ? RUN_HYST_MS : 0);
      bool recent = now - steps.lastStepMs < RUN_HOLD_MS;
      state = (recent && steps.avgMs < runLimit) ? STATE_RUN : STATE_WALK;
    }
    return state;
  }

  // ms left before this shoe calls itself still (0 = already still)
  uint32_t stillIn(uint32_t now, const StepDetector &steps) const {
    if (!steps.lastStepMs || !steps.avgMs || now - steps.lastStepMs >= STILL_MS) return 0;
    return STILL_MS - (now - steps.lastStepMs);
  }
};
