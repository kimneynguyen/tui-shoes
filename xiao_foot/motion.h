#pragma once
// Motion logic: steps, spins, walk / run / still, foot pose, and one-foot events
// (stomp, toe tap, kick, shuffle).
// Plain C++ (no Arduino calls), so tests/logic_test can run it on a laptop.
// Moves that need both feet (jump, hop, skip, lunge, dancer's pose, feet to the
// sky) are worked out in the app, which hears both shoes: sampler/movements.js.
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

// How the shoe is sitting, from the slow gravity direction.
enum FootPose : uint8_t { POSE_FLAT = 0, POSE_TIPTOE = 1, POSE_HEEL = 2, POSE_SIDE = 3, POSE_SKY = 4 };

inline const char *poseName(uint8_t p) {
  switch (p) {
    case POSE_FLAT:   return "FLAT";
    case POSE_TIPTOE: return "TIPTOE";
    case POSE_HEEL:   return "HEEL";
    case POSE_SIDE:   return "SIDE";
    case POSE_SKY:    return "SKY";
  }
  return "?";
}

// One-foot events the shoe sends (packet.event, counted by packet.events).
enum FootEvent : uint8_t { EVENT_NONE = 0, EVENT_STOMP = 1, EVENT_TOE_TAP = 2, EVENT_KICK = 3, EVENT_SHUFFLE = 4 };

inline const char *eventName(uint8_t e) {
  switch (e) {
    case EVENT_NONE:    return "NONE";
    case EVENT_STOMP:   return "STOMP";
    case EVENT_TOE_TAP: return "TOE TAP";
    case EVENT_KICK:    return "KICK";
    case EVENT_SHUFFLE: return "SHUFFLE";
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
  bool touching = false;      // any pressure at all (a foot sliding lightly still counts)
  bool stepped = false;       // true only on the sample a step happened
  uint32_t downSinceMs = 0;   // when the foot last pressed down
  uint32_t upSinceMs = 0;     // when the foot last lifted (0 = never)
  uint16_t lastAirMs = 0;     // how long the foot was up before the last counted step
  float peak = 0;             // learned "full press" level
  float rest = 0;             // learned "foot up" level (an insole or a resting finger can keep it above 0)
  float pressAt = FSR_MIN_PRESS, releaseAt = FSR_MIN_PRESS * 0.6f;

  void beginSample() { stepped = false; }

  void updateFsr(uint32_t now, uint16_t fsr) {
    float dt = lastFsrMs ? (float)(now - lastFsrMs) : 10.0f;
    lastFsrMs = now;
    // full press: jump up to new highs, forget old ones (half gone after FSR_PEAK_HALF_MS)
    peak = fmaxf((float)fsr, peak * powf(0.5f, dt / FSR_PEAK_HALF_MS));
    // foot up level: drops to any lower reading, and slowly rises to what the sensor reads
    // with the foot up. While down it can only rise to half the reading, so standing stays "down".
    float restTarget = down ? fsr * 0.5f : fsr;
    if (fsr < rest) rest = fsr;
    else if (restTarget > rest) rest += (restTarget - rest) * fminf(1.0f, dt / FSR_REST_RISE_MS);
    // press and release are measured from the rest level, so a preloaded sensor still steps
    float span = fmaxf(peak - rest, (float)FSR_MIN_SPAN);
    pressAt = fmaxf(FSR_MIN_PRESS, rest + FSR_PRESS_FRAC * span);
    releaseAt = fmaxf(FSR_MIN_PRESS * 0.6f, rest + FSR_RELEASE_FRAC * span);
    touching = fsr > fmaxf(FSR_MIN_PRESS, rest + FSR_MIN_PRESS);
    if (!down && fsr > pressAt) press(now);
    else if (down && fsr < releaseAt) lift(now);
  }

  void updateAccel(uint32_t now, float accelG) {
    if (!down && accelG > ACCEL_STEP_G) press(now);
    else if (down && accelG < ACCEL_RESET_G) lift(now);
    touching = down;
  }

  uint16_t stepsPerMin() const { return avgMs ? (uint16_t)(60000UL / avgMs) : 0; }
  uint32_t upFor(uint32_t now) const { return (!down && upSinceMs) ? now - upSinceMs : 0; }

private:
  uint16_t prevInterval = 0;
  uint32_t lastFsrMs = 0;

  void press(uint32_t now) {
    down = true;
    downSinceMs = now;
    registerStep(now);
  }
  void lift(uint32_t now) { down = false; upSinceMs = now; }

  void registerStep(uint32_t now) {
    if (lastStepMs && now - lastStepMs < STEP_MIN_MS) return;
    uint32_t air = upSinceMs ? now - upSinceMs : 0;
    lastAirMs = (uint16_t)(air > 60000 ? 60000 : air);
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

// ---------------------------------------------------------------------
// Foot pose: pitch (toes up/down) and roll (sideways), measured from the
// "flat" direction saved when the gyro offset is measured (shoe flat on the floor).
// FORWARD_AXIS / FORWARD_SIGN in config.h say which board axis points to the toes.
// ---------------------------------------------------------------------
class PoseTracker {
public:
  float pitch = 0;            // deg, + toes up, - toes down
  float roll = 0;             // deg, sideways
  float flip = 0;             // deg between "up" now and "up" when flat
  float pitchRate = 0;        // deg/s, + toes going up
  uint8_t pose = POSE_FLAT;
  bool axisOk = true;         // false if FORWARD_AXIS points up/down (then another axis is used)

  PoseTracker() { setFlat(0, 0, 1); }

  void setFlat(float ax, float ay, float az) {
    float n = sqrtf(ax * ax + ay * ay + az * az);
    if (n < 0.5f) { ax = 0; ay = 0; az = 1; n = 1; }
    up[0] = ax / n; up[1] = ay / n; up[2] = az / n;
    axisOk = makeForward(FORWARD_AXIS);
    if (!axisOk) makeForward((FORWARD_AXIS + 1) % 3);
    lat[0] = up[1] * fwd[2] - up[2] * fwd[1];
    lat[1] = up[2] * fwd[0] - up[0] * fwd[2];
    lat[2] = up[0] * fwd[1] - up[1] * fwd[0];
  }

  // gx/gy/gz: slow gravity (GravityTracker). wx/wy/wz: gyro in deg/s.
  void update(uint32_t now, float gx, float gy, float gz, float wx, float wy, float wz) {
    float n = sqrtf(gx * gx + gy * gy + gz * gz);
    if (n > 0.3f) {
      float u[3] = {gx / n, gy / n, gz / n};
      pitch = asinDeg(dot(fwd, u));
      roll = asinDeg(dot(lat, u));
      float c = dot(up, u);
      flip = acosf(c > 1 ? 1 : (c < -1 ? -1 : c)) * 57.2958f;
    }
    float w[3] = {wx, wy, wz};
    pitchRate = -dot(lat, w);

    uint8_t want = classify();
    if (want == pose) { pending = pose; return; }
    if (want != pending) { pending = want; pendingSince = now; }
    else if (now - pendingSince >= POSE_HOLD_MS) pose = want;
  }

private:
  float up[3] = {0, 0, 1}, fwd[3] = {1, 0, 0}, lat[3] = {0, 1, 0};
  uint8_t pending = POSE_FLAT;
  uint32_t pendingSince = 0;

  static float dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
  static float asinDeg(float v) { return asinf(v > 1 ? 1 : (v < -1 ? -1 : v)) * 57.2958f; }

  bool makeForward(int axis) {
    float e[3] = {0, 0, 0};
    e[axis] = FORWARD_SIGN;
    float d = dot(e, up);
    for (int i = 0; i < 3; i++) fwd[i] = e[i] - d * up[i];
    float n = sqrtf(dot(fwd, fwd));
    if (n < 0.5f) return false;   // that axis is (nearly) straight up or down
    for (int i = 0; i < 3; i++) fwd[i] /= n;
    return true;
  }

  // Thresholds are easier to reach than to leave, so the pose doesn't flicker.
  uint8_t classify() const {
    float slack = TILT_DEG - TILT_EXIT_DEG;
    float sky = SKY_PITCH_DEG - (pose == POSE_SKY ? slack : 0);
    float skyFlip = SKY_FLIP_DEG - (pose == POSE_SKY ? slack : 0);
    if (pitch > sky || flip > skyFlip) return POSE_SKY;
    float tilt = (pose == POSE_FLAT || pose == POSE_SKY) ? TILT_DEG : TILT_EXIT_DEG;
    float ap = fabsf(pitch), ar = fabsf(roll);
    if (ap < tilt && ar < tilt) return POSE_FLAT;
    if (ap >= ar) return pitch < 0 ? POSE_TIPTOE : POSE_HEEL;
    return POSE_SIDE;
  }
};

// ---------------------------------------------------------------------
// One-foot events: stomp, toe tap, karate kick, shuffle.
// ---------------------------------------------------------------------
class EventDetector {
public:
  uint8_t last = EVENT_NONE;  // last event
  uint8_t count = 0;          // goes up by 1 on every event
  bool fired = false;         // true only on the sample an event happened
  float info = 0;             // the number that made it fire (g, deg/s or pushes), for debug output
  uint8_t taps = 0;           // taps in the current toe-tap run
  uint8_t pushes = 0;         // pushes in the current shuffle
  float push = 0;             // push along the floor right now (g)

  // gait = MotionClassifier state from the previous sample.
  void update(uint32_t now, uint8_t gait, const StepDetector &steps, const PoseTracker &pose,
              const GravityTracker &grav, float ax, float ay, float az, float gyroDps, float yawDps) {
    fired = false;
    float g = sqrtf(ax * ax + ay * ay + az * az);
    bool running = gait == STATE_RUN;

    // ---- stomp: a hard hit right after the foot presses down ----
    recent[ri] = g; ri = (ri + 1) % 3;
    if (steps.down && steps.downSinceMs == now) {
      stompUntil = now + STOMP_WINDOW_MS;
      stompPeak = fmaxf(recent[0], fmaxf(recent[1], recent[2]));
    }
    if (stompUntil) {
      stompPeak = fmaxf(stompPeak, g);
      if ((int32_t)(now - stompUntil) >= 0) {
        if (stompPeak > STOMP_G && !running) fire(EVENT_STOMP, stompPeak);
        stompUntil = 0;
      }
    }

    // ---- toe tap: toes come down fast and hit, without a real step ----
    if (pose.pitchRate < -TAP_RATE_DPS) toesDownMs = now;
    if (!tapHigh && g > TAP_G && toesDownMs && now - toesDownMs < 120) {
      tapHigh = true;
      uint32_t lifted = steps.down ? (now - steps.downSinceMs < 150 ? steps.lastAirMs : 0) : steps.upFor(now);
      bool wasStep = lifted > TAP_MAX_LIFT_MS;
      if (!running && !wasStep && g < STOMP_G && now - lastTapMs > 150) {
        taps = (lastTapMs && now - lastTapMs < TAP_WINDOW_MS) ? taps + 1 : 1;
        lastTapMs = now;
        if (taps >= TAP_COUNT) fire(EVENT_TOE_TAP, taps);
      }
    } else if (tapHigh && g < (TAP_G + 1.0f) / 2) tapHigh = false;

    // ---- karate kick: foot in the air, swinging very fast and hard ----
    if (steps.down) { kickDone = false; airGyro = 0; airG = 0; }
    else if (steps.upSinceMs) {
      airGyro = fmaxf(airGyro, gyroDps);
      airG = fmaxf(airG, g);
      if (!kickDone && !running && steps.upFor(now) >= KICK_MIN_AIR_MS &&
          airGyro > KICK_GYRO_DPS && airG > KICK_G) {
        kickDone = true;   // once per lift
        fire(EVENT_KICK, airGyro);
      }
    }

    // ---- shuffle: pushes along the floor with the sole touching, no steps ----
    float gn = sqrtf(grav.x * grav.x + grav.y * grav.y + grav.z * grav.z);
    push = 0;
    if (gn > 0.3f) {
      float ux = grav.x / gn, uy = grav.y / gn, uz = grav.z / gn;
      float lx = ax - grav.x, ly = ay - grav.y, lz = az - grav.z;   // movement, without gravity
      float v = lx * ux + ly * uy + lz * uz;
      float hx = lx - v * ux, hy = ly - v * uy, hz = lz - v * uz;  // the part along the floor
      push = sqrtf(hx * hx + hy * hy + hz * hz);
    }
    bool sliding = steps.touching && pose.pose == POSE_FLAT && fabsf(yawDps) < SPIN_RATE_DPS &&
                   !(steps.lastStepMs && now - steps.lastStepMs < SHUFFLE_NO_STEP_MS);
    if (!sliding) { pushes = 0; pushHigh = false; }
    else if (!pushHigh && push > SHUFFLE_G) {
      pushHigh = true;
      pushes = (lastPushMs && now - lastPushMs < SHUFFLE_GAP_MS) ? pushes + 1 : 1;
      lastPushMs = now;
      if (pushes >= SHUFFLE_PUSHES && now - lastShuffleMs >= SHUFFLE_REPEAT_MS) {
        lastShuffleMs = now;
        fire(EVENT_SHUFFLE, pushes);
      }
    } else if (pushHigh && push < SHUFFLE_G * 0.5f) pushHigh = false;
  }

private:
  float recent[3] = {1, 1, 1};
  uint8_t ri = 0;
  uint32_t stompUntil = 0;
  float stompPeak = 0;
  uint32_t toesDownMs = 0, lastTapMs = 0;
  bool tapHigh = false;
  bool kickDone = false;
  float airGyro = 0, airG = 0;
  bool pushHigh = false;
  uint32_t lastPushMs = 0, lastShuffleMs = 0;

  void fire(uint8_t ev, float why) { last = ev; count++; fired = true; info = why; }
};
