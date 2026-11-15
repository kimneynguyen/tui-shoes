/*
  Sound Effect Shoes : Minima foot sketch (one per shoe)
  ------------------------------------------------------
  Board: Arduino UNO R4 Minima

  Figures out what this foot is doing and sends it on:
     STILL  no movement for 2 seconds  -> laptop pauses the sound
     WALK   steps at a normal pace     -> default volume
     RUN    fast steps                 -> max volume
     SPIN   a full 360 turn            -> laptop switches to the next sound

  Files (each one is a tab in the Arduino app):
     config.h      settings: FOOT, parts on/off, thresholds   <<< start here
     fsr_sensor.h  the pressure sensor
     imu.h         the MPU-6050 gyro
     motion.h      steps, spins, walk / run / still
     xiao_link.h   sending to the XIAO
     packet.h      the 20 bytes sent to the XIAO and the laptop

  DEBUGGING: open the Serial Monitor (115200), type a key and press Enter:
     0  app mode: the line the music app reads (default)
     1  pressure sensor only        4  spin detector only
     2  gyro board only             5  walk / run / still decision
     3  step detector only          6  XIAO link only
     c  re-measure the gyro offset (keep the shoe still)
     q  quiet         h  this help
  Modes 1-5 also work in Tools > Serial Plotter.
  To run without a part, set USE_FSR / USE_IMU / USE_XIAO to 0 in config.h.
*/

#include <Wire.h>
#include "config.h"
#include "packet.h"
#include "motion.h"
#include "fsr_sensor.h"
#include "imu.h"
#include "xiao_link.h"

enum Mode : char { MODE_APP = '0', MODE_FSR = '1', MODE_IMU = '2', MODE_STEPS = '3',
                   MODE_SPIN = '4', MODE_STATE = '5', MODE_XIAO = '6', MODE_QUIET = 'q' };

FsrSensor fsr;
Mpu6050 imu;
XiaoLink xiao;
StepDetector steps;
GravityTracker gravity;
SpinDetector spin;
MotionClassifier motion;
FootPacket pkt = {};

char mode = MODE_APP;
float yawDps = 0;
uint32_t lastSample = 0, lastSend = 0, lastImuRetry = 0, lastSlow = 0;
uint8_t lastState = 0xFF;

const bool stepsFromFsr = USE_FSR && STEPS_FROM_FSR;

// The R4's Wire library turns on the chip's internal pull-ups, which go to 5 V.
// The XIAO only takes 3.3 V, so switch them off and let the gyro board's
// 2.2k pull-ups (to 3.3 V) set the level.
void i2cPullupsOff() {
  R_IOPORT_PinCfg(&g_ioport_ctrl, g_pin_cfg[WIRE_SDA_PIN].pin,
                  (uint32_t)(IOPORT_CFG_DRIVE_MID | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_IIC));
  R_IOPORT_PinCfg(&g_ioport_ctrl, g_pin_cfg[WIRE_SCL_PIN].pin,
                  (uint32_t)(IOPORT_CFG_DRIVE_MID | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_IIC));
}

// ---------------------------------------------------------------------
void printHelp() {
  Serial.println(F("# ---- Sound Effect Shoes: debug keys (type, then Enter) ----"));
  Serial.println(F("# 0 app line | 1 pressure | 2 gyro | 3 steps | 4 spin | 5 state | 6 XIAO"));
  Serial.println(F("# c re-measure gyro offset (keep still) | q quiet | h help"));
  Serial.print(F("# parts: FSR ")); Serial.print(USE_FSR ? F("on") : F("OFF"));
  Serial.print(F(", gyro ")); Serial.print(USE_IMU ? (imu.ok ? F("on") : F("NOT FOUND (check SDA/SCL/VCC)")) : F("OFF"));
  Serial.print(F(", XIAO ")); Serial.print(USE_XIAO ? F("on") : F("OFF"));
  Serial.print(F(", steps from ")); Serial.println(stepsFromFsr ? F("pressure sensor") : F("gyro board"));
}

void calibrateGyro() {
#if USE_IMU
  if (!imu.ok) { Serial.println(F("# gyro not found, can't calibrate")); return; }
  Serial.println(F("# measuring gyro offset, keep the shoe still..."));
  imu.calibrate();
  Serial.print(F("# gyro offset x/y/z: "));
  Serial.print(imu.biasX, 2); Serial.print(' ');
  Serial.print(imu.biasY, 2); Serial.print(' ');
  Serial.println(imu.biasZ, 2);
#endif
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (c == 'h' || c == '?') printHelp();
    else if (c == 'c') calibrateGyro();
    else if ((c >= '0' && c <= '6') || c == 'q') {
      mode = c;
      Serial.print(F("# mode ")); Serial.println(c);
    } else {
      Serial.print(F("# unknown key '")); Serial.print(c); Serial.println(F("', type h for help"));
    }
  }
}

// ---------------------------------------------------------------------
void readSensors(uint32_t now, float dt) {
  steps.beginSample();
  bool moving = false;

#if USE_FSR
  fsr.read();
  if (stepsFromFsr) steps.updateFsr(now, fsr.value);
#endif

#if USE_IMU
  if (imu.ok && imu.read()) {
    gravity.update(imu.ax, imu.ay, imu.az);
    yawDps = gravity.yawRate(imu.gx, imu.gy, imu.gz);
    spin.update(now, yawDps, dt);
    if (!stepsFromFsr) steps.updateAccel(now, imu.accelG());
    moving = imu.gyroDps() > MOVE_GYRO_DPS || fabsf(imu.accelG() - 1.0f) > MOVE_ACCEL_G;
  } else if (now - lastImuRetry > 2000) {
    lastImuRetry = now;          // unplugged? keep trying
    if (imu.begin()) Serial.println(F("# gyro found again"));
  }
#endif

  motion.update(now, steps, spin, moving);
}

void fillPacket() {
  pkt.state   = motion.state;
  pkt.spins   = spin.spins;
  pkt.spinPct = spin.progressPct();
  pkt.fsr     = fsr.value;
  pkt.steps   = steps.count;
  pkt.stepMs  = steps.intervalMs;
  pkt.ax      = (int16_t)(imu.ax * 100);
  pkt.ay      = (int16_t)(imu.ay * 100);
  pkt.az      = (int16_t)(imu.az * 100);
  pkt.yawRate = (int16_t)constrain(yawDps, -32000.0f, 32000.0f);
  pkt.gyro    = (uint16_t)constrain(imu.gyroDps(), 0.0f, 65000.0f);
  pkt.flags   = (imu.ok ? FLAG_IMU_OK : 0) | (steps.down ? FLAG_FOOT_DOWN : 0) | (xiao.ok() ? FLAG_XIAO_OK : 0);
}

// ---------------------------------------------------------------------
// Output for each debug mode. "name:value" pairs work in the Serial Plotter.
void printAppLine() {
  // FOOT,state,spins,spinPct,fsr,steps,stepMs,ax,ay,az,yawRate,gyro,flags
  Serial.print(FOOT);           Serial.print(',');
  Serial.print(pkt.state);      Serial.print(',');
  Serial.print(pkt.spins);      Serial.print(',');
  Serial.print(pkt.spinPct);    Serial.print(',');
  Serial.print(pkt.fsr);        Serial.print(',');
  Serial.print(pkt.steps);      Serial.print(',');
  Serial.print(pkt.stepMs);     Serial.print(',');
  Serial.print(pkt.ax);         Serial.print(',');
  Serial.print(pkt.ay);         Serial.print(',');
  Serial.print(pkt.az);         Serial.print(',');
  Serial.print(pkt.yawRate);    Serial.print(',');
  Serial.print(pkt.gyro);       Serial.print(',');
  Serial.println(pkt.flags);
}

void printDebug(uint32_t now) {
  switch (mode) {
    case MODE_APP: printAppLine(); break;
    case MODE_FSR:
      Serial.print(F("fsr:"));        Serial.print(fsr.value);
      Serial.print(F(",pressAt:"));   Serial.print((int)steps.pressAt);
      Serial.print(F(",releaseAt:")); Serial.print((int)steps.releaseAt);
      Serial.print(F(",down:"));      Serial.println(steps.down ? 1000 : 0);
      break;
    case MODE_IMU:
      Serial.print(F("ax:"));    Serial.print(imu.ax, 2);
      Serial.print(F(",ay:"));   Serial.print(imu.ay, 2);
      Serial.print(F(",az:"));   Serial.print(imu.az, 2);
      Serial.print(F(",g:"));    Serial.print(imu.accelG(), 2);
      Serial.print(F(",gx:"));   Serial.print(imu.gx, 0);
      Serial.print(F(",gy:"));   Serial.print(imu.gy, 0);
      Serial.print(F(",gz:"));   Serial.print(imu.gz, 0);
      Serial.print(F(",yaw:"));  Serial.print(yawDps, 0);
      Serial.print(F(",found:"));Serial.println(imu.ok ? 1 : 0);
      break;
    case MODE_STEPS:
      Serial.print(stepsFromFsr ? F("fsr:") : F("g100:"));
      Serial.print(stepsFromFsr ? (float)fsr.value : imu.accelG() * 100, 0);
      Serial.print(F(",down:"));    Serial.print(steps.down ? 1 : 0);
      Serial.print(F(",steps:"));   Serial.print(steps.count);
      Serial.print(F(",stepMs:"));  Serial.print(steps.intervalMs);
      Serial.print(F(",avgMs:"));   Serial.print(steps.avgMs);
      Serial.print(F(",perMin:"));  Serial.println(steps.stepsPerMin());
      break;
    case MODE_SPIN:
      Serial.print(F("yaw:"));      Serial.print(yawDps, 0);
      Serial.print(F(",angle:"));   Serial.print(spin.angle, 0);
      Serial.print(F(",pct:"));     Serial.print(spin.progressPct());
      Serial.print(F(",spins:"));   Serial.println(spin.spins);
      break;
    case MODE_STATE:
      Serial.print(F("state:"));    Serial.print(motion.state);
      Serial.print(F(",avgStepMs:"));Serial.print(steps.avgMs);
      Serial.print(F(",runBelow:"));Serial.print(RUN_STEP_MS);
      Serial.print(F(",stillInMs:"));Serial.print(motion.stillIn(now));
      Serial.print(F(",spinPct:")); Serial.print(spin.progressPct());
      Serial.print(F("  "));        Serial.println(stateName(motion.state));
      break;
    case MODE_XIAO:
      if (now - lastSlow < 500) break;
      lastSlow = now;
#if USE_XIAO
      Serial.print(F("# xiao: "));
      Serial.print(xiao.ok() ? F("ok") : F("NOT ANSWERING (check D4/D5 wires and XIAO power)"));
      Serial.print(F("  sent:"));   Serial.print(xiao.sent);
      Serial.print(F("  failed:")); Serial.print(xiao.failed);
      Serial.print(F("  last error code:")); Serial.println(xiao.lastErr);
#else
      Serial.println(F("# xiao: OFF (USE_XIAO is 0 in config.h)"));
#endif
      break;
  }
}

void printEvents() {
  if (steps.stepped && (mode == MODE_FSR || mode == MODE_STEPS)) {
    Serial.print(F("# step ")); Serial.print(steps.count);
    Serial.print(F(" (")); Serial.print(steps.intervalMs); Serial.println(F(" ms)"));
  }
  bool showEvents = mode == MODE_APP || mode == MODE_SPIN || mode == MODE_STATE;
  if (spin.spun && showEvents) {
    Serial.print(F("# ")); Serial.print(FOOT);
    Serial.print(F(" SPIN #")); Serial.println(spin.spins);
  }
  if (motion.state != lastState) {
    if (showEvents) {
      Serial.print(F("# ")); Serial.print(FOOT);
      Serial.print(F(" state ")); Serial.println(stateName(motion.state));
    }
    lastState = motion.state;
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
#if USE_FSR
  fsr.begin();
#endif
#if USE_IMU || USE_XIAO
  Wire.begin();
  Wire.setClock(100000);
  i2cPullupsOff();
#endif
#if USE_IMU
  imu.begin();
#endif
  delay(300);
  Serial.print(F("# Sound Effect Shoes, Minima foot ")); Serial.println(FOOT);
  calibrateGyro();
  printHelp();
  lastSample = millis();
}

void loop() {
  handleSerial();

  uint32_t now = millis();
  if (now - lastSample < 1000 / SAMPLE_HZ) return;
  float dt = (now - lastSample) / 1000.0f;
  lastSample = now;

  readSensors(now, dt);
  printEvents();

  if (now - lastSend >= 1000 / SEND_HZ) {
    lastSend = now;
    fillPacket();
#if USE_XIAO
    xiao.send(pkt);
#endif
    printDebug(now);
  }
}
