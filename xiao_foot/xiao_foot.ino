/*
  Sound Effect Shoes : XIAO foot sketch (one per shoe, no Minima)
  ----------------------------------------------------------------
  Board: Seeed XIAO ESP32-C6 (IDE board "XIAO_ESP32C6"), esp32 board package 3.x.
         Tools > USB CDC On Boot: Enabled.
  Wiring: docs/xiao-only-rewiring.md. FSR on D0 (10k to GND), gyro on D4 SDA / D5 SCL,
          everything on 3.3 V from the XIAO's 3V3 pin. Nothing on the 5V pin.

  Reads the sensors, works out what this foot is doing, and sends it to the
  laptop over Bluetooth as "MusicShoe-L" or "MusicShoe-R":
     STILL  no steps for 2 seconds      -> laptop pauses the sound
     WALK   steps at a normal pace      -> default volume
     RUN    fast steps                  -> max volume
     SPIN   a full 360 turn             -> laptop switches to the next sound

  Files (each one is a tab in the Arduino app):
     config.h      settings: FOOT, parts on/off, thresholds   <<< start here
     fsr_sensor.h  the pressure sensor
     imu.h         the MPU-6050 gyro
     motion.h      steps, spins, walk / run / still
     packet.h      the 20 bytes sent to the laptop

  DEBUGGING: open the Serial Monitor (115200), type a key and press Enter:
     0  app mode: the line the music app reads over USB (default)
     1  pressure sensor only        4  spin detector only
     2  gyro board only             5  walk / run / still decision
     3  step detector only          6  Bluetooth status
     c  re-measure the gyro offset (keep the shoe still)
     q  quiet         h  this help
  Modes 1-5 also work in Tools > Serial Plotter.
  To run without a part, set USE_FSR / USE_IMU to 0 in config.h.
  To test Bluetooth and the app with no sensors, set DEMO_MODE to 1.
*/

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "config.h"
#include "packet.h"
#include "motion.h"
#include "fsr_sensor.h"
#include "imu.h"

// Must match the app (sampler/index.html)
#define SERVICE_UUID "7b1e0001-5d2c-4c3b-9a41-50a1c5717e00"
#define DATA_UUID    "7b1e0002-5d2c-4c3b-9a41-50a1c5717e00"

enum Mode : char { MODE_APP = '0', MODE_FSR = '1', MODE_IMU = '2', MODE_STEPS = '3',
                   MODE_SPIN = '4', MODE_STATE = '5', MODE_BLE = '6', MODE_QUIET = 'q' };

FsrSensor fsr;
Mpu6050 imu;
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

// ---------------------------------------------------------------------
// Bluetooth
// ---------------------------------------------------------------------
BLECharacteristic *dataChar = nullptr;
volatile bool connected = false;
uint32_t notifications = 0;

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) override { connected = true; }
  void onDisconnect(BLEServer *s) override {
    connected = false;
    BLEDevice::startAdvertising();   // let the laptop reconnect
  }
};

String bleName() { return String("MusicShoe-") + FOOT; }

void startBluetooth() {
  BLEDevice::init(bleName().c_str());
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *svc = server->createService(SERVICE_UUID);
  dataChar = svc->createCharacteristic(DATA_UUID,
               BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  dataChar->addDescriptor(new BLE2902());
  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void sendBluetooth() {
  if (!connected) return;
  dataChar->setValue((uint8_t *)&pkt, sizeof(pkt));
  dataChar->notify();
  notifications++;
}

// ---------------------------------------------------------------------
void printHelp() {
  Serial.println(F("# ---- Sound Effect Shoes: debug keys (type, then Enter) ----"));
  Serial.println(F("# 0 app line | 1 pressure | 2 gyro | 3 steps | 4 spin | 5 state | 6 Bluetooth"));
  Serial.println(F("# c re-measure gyro offset (keep still) | q quiet | h help"));
#if DEMO_MODE
  Serial.println(F("# DEMO MODE: sensors not read, made-up movement (DEMO_MODE in config.h)"));
#endif
  Serial.print(F("# parts: FSR ")); Serial.print(USE_FSR ? F("on (D0)") : F("OFF"));
  Serial.print(F(", gyro ")); Serial.print(USE_IMU ? (imu.ok ? F("on") : F("NOT FOUND (check D4 SDA / D5 SCL / VCC / GND)")) : F("OFF"));
  Serial.print(F(", steps from ")); Serial.println(stepsFromFsr ? F("pressure sensor") : F("gyro board"));
  Serial.print(F("# Bluetooth name ")); Serial.println(bleName());
}

void calibrateGyro() {
#if USE_IMU && !DEMO_MODE
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
  } else if (now - lastImuRetry > 2000) {
    lastImuRetry = now;          // unplugged? keep trying
    if (imu.begin()) Serial.println(F("# gyro found again"));
  }
#endif

  motion.update(now, steps, spin);
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
  pkt.flags   = (imu.ok ? FLAG_IMU_OK : 0) | (steps.down ? FLAG_FOOT_DOWN : 0) | FLAG_XIAO_OK;
}

#if DEMO_MODE
// Cycles STILL -> WALK -> RUN -> SPIN every 4 seconds.
void makeDemoPacket(uint32_t now) {
  static uint8_t lastPhase = 0xFF;
  static uint32_t lastStep = 0;
  uint8_t phase = (now / 4000) % 4;
  pkt.state = phase;
  uint16_t stepEvery = phase == 1 ? 1100 : 700;
  if ((phase == 1 || phase == 2) && now - lastStep > stepEvery) {
    lastStep = now; pkt.steps++; pkt.stepMs = stepEvery;
  }
  if (phase == 3 && lastPhase != 3) pkt.spins++;
  lastPhase = phase;
  pkt.spinPct = phase == 3 ? 100 : 0;
  pkt.fsr = (phase == 1 || phase == 2) ? ((now - lastStep < 300) ? 2500 : 100) : (phase == 0 ? 1800 : 1200);
  pkt.ax = 0; pkt.ay = 0; pkt.az = 100;
  pkt.yawRate = phase == 3 ? 500 : 0;
  pkt.gyro = phase == 0 ? 2 : (phase == 3 ? 500 : 150);
  pkt.flags = FLAG_IMU_OK | FLAG_XIAO_OK;
}
#endif

// ---------------------------------------------------------------------
// Output for each debug mode. "name:value" pairs work in the Serial Plotter.
void printAppLine() {
  // FOOT,state,spins,spinPct,fsr,steps,stepMs,ax,ay,az,yawRate,gyro,flags
  Serial.printf("%c,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%u,%u\n", FOOT,
                pkt.state, pkt.spins, pkt.spinPct, pkt.fsr, pkt.steps, pkt.stepMs,
                pkt.ax, pkt.ay, pkt.az, pkt.yawRate, pkt.gyro, pkt.flags);
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
      Serial.print(F(",stillInMs:"));Serial.print(motion.stillIn(now, steps));
      Serial.print(F(",spinPct:")); Serial.print(spin.progressPct());
      Serial.print(F("  "));        Serial.println(stateName(motion.state));
      break;
    case MODE_BLE:
      if (now - lastSlow < 1000) break;
      lastSlow = now;
      Serial.print(F("# bluetooth ")); Serial.print(bleName());
      Serial.print(connected ? F(": connected") : F(": waiting for the laptop"));
      Serial.print(F("  sent:")); Serial.print(notifications);
      Serial.print(F("  state=")); Serial.println(stateName(pkt.state));
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
  Serial.setTxTimeoutMs(0);   // never wait on USB: with no Serial Monitor open, printing would stall
#if USE_FSR && !DEMO_MODE
  fsr.begin();
  pinMode(FSR_PIN, INPUT);
#endif
#if USE_IMU && !DEMO_MODE
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  imu.begin();
#endif
  startBluetooth();
  delay(300);
  Serial.print(F("# Sound Effect Shoes, XIAO foot ")); Serial.println(FOOT);
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

#if DEMO_MODE
  makeDemoPacket(now);
#else
  readSensors(now, dt);
  printEvents();
#endif

  if (now - lastSend >= 1000 / SEND_HZ) {
    lastSend = now;
#if !DEMO_MODE
    fillPacket();
#endif
    sendBluetooth();
    if (Serial.availableForWrite() >= 64) printDebug(now);   // skip if nobody is reading USB
  }
}
