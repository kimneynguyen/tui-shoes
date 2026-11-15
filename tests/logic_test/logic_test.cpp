// Runs minima_foot/motion.h on the laptop with made-up sensor data.
// Build and run:  g++ -std=c++17 -I../../minima_foot logic_test.cpp -o logic_test && ./logic_test
#include <cstdio>
#include "motion.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) printf("  ok    %s\n", msg); else { printf("  FAIL  %s\n", msg); failures++; } } while (0)

struct Shoe {
  StepDetector steps; GravityTracker gravity; SpinDetector spin; MotionClassifier motion;
  uint32_t t = 0;
  bool fsrSteps = true;

  // one 10 ms sample, same order as minima_foot.ino readSensors()
  void sample(uint16_t fsr, float ax, float ay, float az, float gx, float gy, float gz) {
    t += 10;
    steps.beginSample();
    if (fsrSteps) steps.updateFsr(t, fsr);
    gravity.update(ax, ay, az);
    float yaw = gravity.yawRate(gx, gy, gz);
    spin.update(t, yaw, 0.01f);
    float g = sqrtf(ax * ax + ay * ay + az * az), gyro = sqrtf(gx * gx + gy * gy + gz * gz);
    if (!fsrSteps) steps.updateAccel(t, g);
    bool moving = gyro > MOVE_GYRO_DPS || fabsf(g - 1.0f) > MOVE_ACCEL_G;
    motion.update(t, steps, spin, moving);
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
  const char *st() const { return stateName(motion.state); }
};

int main() {
  printf("Standing still\n");
  { Shoe s; s.still(3000); CHECK(s.motion.state == STATE_STILL, "still after 3 s of standing"); }

  printf("Walking (one foot steps every 1100 ms)\n");
  { Shoe s; s.still(2500); s.gait(1100, 600, 6000);
    printf("        state=%s steps=%u avgMs=%u\n", s.st(), s.steps.count, s.steps.avgMs);
    CHECK(s.motion.state == STATE_WALK, "WALK");
    CHECK(s.steps.count >= 5, "counted the steps"); }

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

  printf("\n%s (%d failed)\n", failures ? "SOME TESTS FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}
