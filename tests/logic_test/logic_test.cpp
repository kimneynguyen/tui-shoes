// Runs xiao_foot/motion.h on the laptop with made-up sensor data.
// Build and run:  g++ -std=c++17 -I../../xiao_foot logic_test.cpp -o logic_test && ./logic_test
// The made-up data assumes the board lies flat with z up and x pointing to the toes
// (FORWARD_AXIS 0, FORWARD_SIGN 1), which is the default in config.h.
#include <cstdio>
#include "motion.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) printf("  ok    %s\n", msg); else { printf("  FAIL  %s\n", msg); failures++; } } while (0)

struct Shoe {
  StepDetector steps; GravityTracker gravity; SpinDetector spin; MotionClassifier motion;
  PoseTracker pose; EventDetector events;
  uint32_t t = 0;
  bool fsrSteps = true;
  int counts[5] = {0, 0, 0, 0, 0};   // events seen, by EVENT_*

  // one 10 ms sample, same order as xiao_foot.ino readSensors()
  void sample(uint16_t fsr, float ax, float ay, float az, float gx, float gy, float gz) {
    t += 10;
    steps.beginSample();
    if (fsrSteps) steps.updateFsr(t, fsr);
    gravity.update(ax, ay, az);
    float yaw = gravity.yawRate(gx, gy, gz);
    spin.update(t, yaw, 0.01f);
    pose.update(t, gravity.x, gravity.y, gravity.z, gx, gy, gz);
    float g = sqrtf(ax * ax + ay * ay + az * az);
    if (!fsrSteps) steps.updateAccel(t, g);
    events.update(t, motion.state, steps, pose, gravity, ax, ay, az, sqrtf(gx * gx + gy * gy + gz * gz), yaw);
    if (events.fired) counts[events.last]++;
    motion.update(t, steps, spin);
  }
  void still(uint32_t ms, uint16_t fsr = 1800) { for (uint32_t i = 0; i < ms; i += 10) sample(fsr, 0, 0, 1, 0, 0, 0); }
  // walk/run: foot down for downMs, then swings (rotating about the side-to-side axis)
  void gait(uint32_t periodMs, uint32_t downMs, uint32_t totalMs, float impactG = 1.0f) {
    for (uint32_t i = 0; i < totalMs; i += 10) {
      uint32_t p = i % periodMs;
      bool down = p < downMs;
      float az = (p < 30) ? impactG : 1.0f;
      sample(down ? 2000 : 40, 0, 0, az, down ? 0 : 150, 0, 0);
    }
  }
  // turn around the up/down line at dps for ms. upAxis 2 = z is up, 0 = x is up (board on its side)
  void turn(float dps, uint32_t ms, int upAxis = 2) {
    for (uint32_t i = 0; i < ms; i += 10) {
      if (upAxis == 2) sample(1500, 0, 0, 1, 0, 0, dps);
      else sample(1500, 1, 0, 0, dps, 0, 0);
    }
  }
  // hold the shoe at a pitch (+ toes up) and roll, in degrees
  void hold(float pitchDeg, float rollDeg, uint32_t ms, uint16_t fsr = 1800) {
    float p = pitchDeg / 57.2958f, r = rollDeg / 57.2958f;
    for (uint32_t i = 0; i < ms; i += 10) sample(fsr, sinf(p), sinf(r) * cosf(p), cosf(r) * cosf(p), 0, 0, 0);
  }
  const char *st() const { return stateName(motion.state); }
};

int main() {
  printf("Standing still\n");
  { Shoe s; s.still(3000); CHECK(s.motion.state == STATE_STILL, "still after 3 s of standing"); }

  printf("Walking (one foot steps every 1100 ms)\n");
  { Shoe s; s.still(2500); s.gait(1100, 600, 6000);
    printf("        state=%s steps=%u avgMs=%u\n", s.st(), s.steps.count, s.steps.avgMs);
    CHECK(s.motion.state == STATE_WALK, "WALK");
    CHECK(s.steps.count >= 5, "counted the steps");
    CHECK(s.steps.lastAirMs >= 400 && s.steps.lastAirMs <= 600, "air time before each step is the swing (~500 ms)"); }

  printf("Running (one foot steps every 700 ms)\n");
  { Shoe s; s.still(2500); s.gait(700, 250, 5000, 3.0f);
    printf("        state=%s avgMs=%u\n", s.st(), s.steps.avgMs);
    CHECK(s.motion.state == STATE_RUN, "RUN"); }

  printf("Walk -> run -> walk\n");
  { Shoe s; s.still(2500); s.gait(1100, 600, 4000); CHECK(s.motion.state == STATE_WALK, "starts WALK");
    s.gait(700, 250, 3000); CHECK(s.motion.state == STATE_RUN, "speeds up to RUN");
    s.gait(1100, 600, 4000); CHECK(s.motion.state == STATE_WALK, "slows back to WALK"); }

  printf("Stopping: still only after 2 s\n");
  { Shoe s; s.still(2500); s.gait(1100, 600, 4400);
    s.still(1500); CHECK(s.motion.state != STATE_STILL, "not still yet after 1.5 s");
    s.still(800);  CHECK(s.motion.state == STATE_STILL, "still after 2.3 s"); }

  printf("Moving the shoe without stepping must stay STILL\n");
  { Shoe s; s.still(2500, 0);
    for (int i = 0; i < 400; i++) { float a = 0.6f * sinf(i * 0.08f); s.sample(0, sinf(a), 0, cosf(a), (i % 80 < 40) ? 150 : -150, 0, 0); }
    CHECK(s.motion.state == STATE_STILL, "tilting back and forth for 4 s, sole not pressed"); }
  { Shoe s; s.still(2500, 0);
    for (int i = 0; i < 300; i++) s.sample(0, 0.3f * ((i % 6) - 3), 0, 1 + 0.5f * ((i % 4) - 2), 200, 120, 0);
    CHECK(s.motion.state == STATE_STILL, "shaking it in the air"); }
  { Shoe s; s.still(2500, 0); s.still(300, 2000); s.still(1000, 0);
    CHECK(s.motion.state == STATE_STILL, "one press is not walking"); }
  { Shoe s; s.still(2500, 1800);
    for (int i = 0; i < 300; i++) { float a = 0.3f * sinf(i * 0.05f); s.sample(1800, sinf(a), 0, cosf(a), 80 * cosf(i * 0.05f), 0, 0); }
    CHECK(s.motion.state == STATE_STILL, "standing on it and rocking (pressure stays on)"); }

  printf("360 spin\n");
  { Shoe s; s.still(2500); s.turn(500, 800);
    CHECK(s.spin.spins == 1, "one spin counted (400 deg at 500 deg/s)");
    CHECK(s.motion.state == STATE_SPIN, "state SPIN");
    s.still(2500); CHECK(s.motion.state == STATE_STILL, "back to still afterwards"); }

  printf("Two spins in a row, other direction\n");
  { Shoe s; s.still(2500); s.turn(-450, 1700);
    CHECK(s.spin.spins == 2, "two spins counted"); }

  printf("Spin with the board on its side (x axis is up)\n");
  { Shoe s; s.still(2500, 1500); for (int i = 0; i < 300; i++) s.sample(1500, 1, 0, 0, 0, 0, 0);
    s.turn(500, 800, 0); CHECK(s.spin.spins == 1, "spin counted"); }

  printf("Things that must NOT be a spin\n");
  { Shoe s; s.still(2500); s.turn(300, 600); s.still(500);
    CHECK(s.spin.spins == 0, "half turn (180 deg)"); }
  { Shoe s; s.still(2500); for (int k = 0; k < 8; k++) s.turn(k % 2 ? -300 : 300, 300);
    CHECK(s.spin.spins == 0, "twisting back and forth"); }
  { Shoe s; s.still(2500); s.turn(100, 4000);
    CHECK(s.spin.spins == 0, "slow turn (walking in a circle)"); }
  { Shoe s; s.still(2500); s.gait(1100, 600, 6000);
    CHECK(s.spin.spins == 0, "walking straight"); }

  printf("Steps from the gyro board instead of the pressure sensor\n");
  { Shoe s; s.fsrSteps = false; s.still(2500); s.gait(700, 250, 5000, 3.0f);
    printf("        state=%s steps=%u avgMs=%u\n", s.st(), s.steps.count, s.steps.avgMs);
    CHECK(s.motion.state == STATE_RUN, "RUN from impacts"); }

  printf("Foot pose (tilt and sky)\n");
  { Shoe s; s.still(1000); CHECK(s.pose.pose == POSE_FLAT, "flat on the floor"); }
  { Shoe s; s.still(500); s.hold(-30, 0, 1500);
    printf("        pitch=%.0f roll=%.0f\n", s.pose.pitch, s.pose.roll);
    CHECK(s.pose.pose == POSE_TIPTOE, "toes down 30 deg = TIPTOE"); }
  { Shoe s; s.still(500); s.hold(30, 0, 1500); CHECK(s.pose.pose == POSE_HEEL, "toes up 30 deg = HEEL"); }
  { Shoe s; s.still(500); s.hold(0, 35, 1500); CHECK(s.pose.pose == POSE_SIDE, "rolled 35 deg = SIDE"); }
  { Shoe s; s.still(500); s.hold(0, -35, 1500); CHECK(s.pose.pose == POSE_SIDE, "rolled the other way = SIDE"); }
  { Shoe s; s.still(500); s.hold(80, 0, 1500, 0); CHECK(s.pose.pose == POSE_SKY, "toes pointing up 80 deg = SKY"); }
  { Shoe s; s.still(500); s.hold(0, 170, 1500, 0); CHECK(s.pose.pose == POSE_SKY, "turned over, sole up = SKY"); }
  { Shoe s; s.still(500); s.hold(10, 8, 1500); CHECK(s.pose.pose == POSE_FLAT, "a small 10 deg lean stays FLAT"); }
  { Shoe s; s.still(500); s.hold(-30, 0, 120); s.still(1000); CHECK(s.pose.pose == POSE_FLAT, "a quick wobble doesn't change the pose"); }
  { Shoe s; s.still(500); s.hold(-30, 0, 1500); s.hold(-16, 0, 800);
    CHECK(s.pose.pose == POSE_TIPTOE, "stays TIPTOE until under TILT_EXIT_DEG"); s.hold(-5, 0, 1000);
    CHECK(s.pose.pose == POSE_FLAT, "back to FLAT"); }

  printf("Stomp\n");
  { Shoe s; s.still(2000); s.still(300, 0);
    s.sample(2500, 0, 0, 5.0f, 0, 0, 0); s.still(300);
    CHECK(s.counts[EVENT_STOMP] == 1, "lift, then land with a 5 g hit = STOMP"); }
  { Shoe s; s.still(2500); s.gait(1100, 600, 6000, 1.6f);
    CHECK(s.counts[EVENT_STOMP] == 0, "normal walking (1.6 g steps) is not a stomp"); }
  { Shoe s; s.still(2500); s.gait(700, 250, 5000, 5.0f);
    CHECK(s.counts[EVENT_STOMP] <= 2, "hard running steps don't keep stomping (only before RUN is known)"); }

  printf("Karate kick\n");
  { Shoe s; s.still(2000); s.still(200, 0);
    for (int i = 0; i < 15; i++) s.sample(0, 2.0f, 0, 2.0f, 0, 900, 0);
    s.still(500, 0); s.still(300);
    CHECK(s.counts[EVENT_KICK] == 1, "foot up, swinging 900 deg/s with 2.8 g = one KICK"); }
  { Shoe s; s.still(2500); s.gait(1100, 600, 6000);
    CHECK(s.counts[EVENT_KICK] == 0, "walking swings are not kicks"); }
  { Shoe s; s.still(2000);
    for (int i = 0; i < 15; i++) s.sample(1800, 2.0f, 0, 2.0f, 0, 900, 0);
    CHECK(s.counts[EVENT_KICK] == 0, "foot on the ground is never a kick"); }

  // A tap: toes lift (pitch up), then come down fast and hit, heel stays pressed.
  auto tap = [](Shoe &s) {
    for (int i = 0; i < 15; i++) s.sample(1800, 0.25f, 0, 0.97f, 0, -80, 0);   // toes going up
    for (int i = 0; i < 8; i++)  s.sample(1800, 0.15f, 0, 0.99f, 0, 250, 0);   // toes coming down fast
    s.sample(1800, 0, 0, 1.9f, 0, 100, 0);                                      // hit
    s.still(250);
  };
  printf("Toe tap (heel stays down)\n");
  { Shoe s; s.still(2000); tap(s);
    CHECK(s.counts[EVENT_TOE_TAP] == 0, "one tap is not enough");
    tap(s); CHECK(s.counts[EVENT_TOE_TAP] == 1, "second tap = TOE TAP");
    tap(s); CHECK(s.counts[EVENT_TOE_TAP] == 2, "each further tap = TOE TAP again"); }
  { Shoe s; s.still(2000); tap(s); s.still(1500); tap(s);
    CHECK(s.counts[EVENT_TOE_TAP] == 0, "two taps 1.5 s apart don't count"); }
  { Shoe s; s.still(2500);
    for (int k = 0; k < 6; k++) {   // walking with a foot slap: heel strike, then toes come down hard
      s.still(500, 40);
      s.sample(2000, 0, 0, 1.2f, 0, 0, 0);
      for (int i = 0; i < 5; i++) s.sample(2000, 0.1f, 0, 1.0f, 0, 250, 0);
      s.sample(2000, 0, 0, 1.9f, 0, 0, 0);
      s.still(500, 2000);
    }
    CHECK(s.counts[EVENT_TOE_TAP] == 0, "walking foot slaps are not toe taps"); }

  printf("Pressure sensor: press and release levels\n");
  { // presses of `press` (300 ms) with `rest` between (400 ms), after one squeeze to `hard`
    auto presses = [](uint16_t rest, uint16_t hard, uint16_t press) {
      StepDetector s; uint32_t t = 0;
      auto feed = [&](uint16_t v, int ms) { for (int i = 0; i < ms; i += 10) { t += 10; s.beginSample(); s.updateFsr(t, v); } };
      feed(rest, 2000); feed(hard, 300); feed(rest, 700);
      uint16_t c = s.count;
      for (int i = 0; i < 10; i++) { feed(press, 300); feed(rest, 400); }
      printf("        rest %u, squeeze %u, 10 presses of %u: %u counted (press at %.0f, release at %.0f)\n", rest, hard, press, s.count - c, s.pressAt, s.releaseAt);
      return s.count - c;
    };
    CHECK(presses(30, 4095, 1200) >= 8, "one hard squeeze doesn't hide lighter presses for long");
    CHECK(presses(900, 4095, 3000) == 10, "a sensor that never unloads (finger resting, 900) still counts presses");
    CHECK(presses(1000, 4095, 2500) == 10, "insole preload (1000) still counts steps");
    StepDetector s; uint32_t t = 0; bool allDown = true;
    auto feed = [&](uint16_t v, int ms) { for (int i = 0; i < ms; i += 10) { t += 10; s.beginSample(); s.updateFsr(t, v); } };
    feed(40, 1000); feed(2000, 300);
    uint16_t c = s.count;
    for (int i = 0; i < 6000; i++) { t += 10; s.beginSample(); s.updateFsr(t, 2000 + ((i / 37) % 2 ? 150 : -150)); allDown &= s.down; }
    CHECK(allDown && s.count == c, "standing 60 s with weight shifts: stays down, no extra steps");
    feed(40, 300);
    CHECK(!s.down, "then lifting the foot: up"); }

  printf("Air time (accelerometer only)\n");
  { AirDetector a; uint32_t t = 0;
    auto feed = [&](float g, int ms) { for (int i = 0; i < ms; i += 10) { t += 10; a.update(t, g); } };
    feed(1.0f, 1000); feed(0.15f, 400); feed(2.5f, 20); feed(1.0f, 500);
    CHECK(!a.airborne && a.lastAirMs >= 380 && a.lastAirMs <= 420, "jump: 400 ms weightless then a landing hit = ~400 ms air time");
    uint16_t before = a.lastAirMs;
    for (int k = 0; k < 20; k++) { feed(1.0f, 300); feed(0.7f, 200); feed(1.6f, 30); }
    CHECK(a.lastAirMs == before, "walking (never weightless) adds no air time");
    feed(0.3f, 20); feed(1.0f, 200);
    CHECK(a.lastAirMs == before && !a.airborne, "a 20 ms dip is a bump, not air");
    feed(0.1f, 2500); feed(1.0f, 100);
    CHECK(!a.airborne && a.lastAirMs == before, "weightless for 2.5 s (dropped shoe) is not air time");
    feed(0.2f, 300); feed(1.05f, 300);
    CHECK(!a.airborne && a.lastAirMs >= 290 && a.lastAirMs <= 320, "soft landing (no hard hit): still ends, ~300 ms"); }

  printf("Shuffle / slide\n");
  { Shoe s; s.still(2000, 600);
    for (int k = 0; k < 8; k++) {   // slide forward and back, sole touching lightly
      float a = (k % 2) ? -0.6f : 0.6f;
      for (int i = 0; i < 15; i++) s.sample(600, a, 0, 1, 0, 0, 0);
      for (int i = 0; i < 15; i++) s.sample(600, 0, 0, 1, 0, 0, 0);
    }
    printf("        pushes=%u\n", s.events.pushes);
    CHECK(s.counts[EVENT_SHUFFLE] >= 1, "sliding back and forth = SHUFFLE"); }
  { Shoe s; s.still(2000, 0);
    for (int k = 0; k < 8; k++) {
      float a = (k % 2) ? -0.6f : 0.6f;
      for (int i = 0; i < 15; i++) s.sample(0, a, 0, 1, 0, 0, 0);
      for (int i = 0; i < 15; i++) s.sample(0, 0, 0, 1, 0, 0, 0);
    }
    CHECK(s.counts[EVENT_SHUFFLE] == 0, "the same movement in the air is not a shuffle"); }
  { Shoe s; s.still(2500); s.gait(1100, 600, 6000);
    CHECK(s.counts[EVENT_SHUFFLE] == 0, "walking is not a shuffle"); }
  { Shoe s; s.still(3000);
    CHECK(s.counts[EVENT_SHUFFLE] + s.counts[EVENT_STOMP] + s.counts[EVENT_KICK] + s.counts[EVENT_TOE_TAP] == 0,
          "standing still: no events at all"); }

  printf("\n%s (%d failed)\n", failures ? "SOME TESTS FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}
