/*
  Sound Effect Shoes : XIAO foot sketch (one per shoe, no Minima)
  ----------------------------------------------------------------
  Board: Seeed XIAO ESP32-C6 (IDE board "XIAO_ESP32C6"), esp32 board package 3.x.
         Tools > USB CDC On Boot: Enabled.
  Wiring: docs/xiao-only-rewiring.md. FSR on D0 (10k to GND), gyro on D4 SDA / D5 SCL,
          everything on 3.3 V from the XIAO's 3V3 pin. Nothing on the 5V pin.

  Reads the sensors, works out what this foot is doing, and sends it to the
  laptop over Bluetooth as "MusicShoe-L" or "MusicShoe-R". This foot reports:
     gait   STILL / WALK / RUN / SPIN (steps and 360 turns)
     pose   FLAT / TIPTOE / HEEL / SIDE / SKY (how the shoe is tilted)
     events STOMP, TOE TAP, KICK, SHUFFLE
     plus when it presses down and how long it was in the air before,
  so the app can combine both feet into jump, hop, skip, lunge, dancer's pose
  and feet to the sky (sampler/movements.js).

  Files (each one is a tab in the Arduino app):
     config.h      settings: FOOT, parts on/off, thresholds   <<< start here
     fsr_sensor.h  the pressure sensor
     imu.h         the MPU-6050 gyro
     motion.h      steps, spins, walk / run / still, pose, one-foot events
     packet.h      the 20 bytes sent to the laptop

  DEBUGGING: open the Serial Monitor (115200), type a key and press Enter:
     0  app mode: the line the music app reads over USB (default)
     1  pressure sensor only        5  walk / run / still decision
     2  gyro board only             6  Bluetooth status
     3  step detector only          7  movements: pose, pitch, roll, events
     4  spin detector only
     c  re-measure the gyro offset and "flat" (keep the shoe flat and still)
     q  quiet         h  this help
  Every event prints a line like "# L STOMP (4.2)" in modes 0, 5 and 7.
  Modes 1-5 and 7 also work in Tools > Serial Plotter.
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
#define FOOT_UUID    "7b1e0003-5d2c-4c3b-9a41-50a1c5717e00"

enum Mode : char { MODE_APP = '0', MODE_FSR = '1', MODE_IMU = '2', MODE_STEPS = '3',
                   MODE_SPIN = '4', MODE_STATE = '5', MODE_BLE = '6', MODE_MOVES = '7',
                   MODE_QUIET = 'q' };

FsrSensor fsr;
Mpu6050 imu;
StepDetector steps;
GravityTracker gravity;
SpinDetector spin;
MotionClassifier motion;
PoseTracker pose;
EventDetector events;
FootPacket pkt = {};

char mode = MODE_APP;
float yawDps = 0, peakG = 0;
uint32_t lastSample = 0, lastSend = 0, lastImuRetry = 0, lastSlow = 0;
uint8_t lastState = 0xFF, lastPose = 0xFF;

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
  // "L" or "R": the app reads this after connecting, because macOS can list a
  // shoe under an old cached name.
  BLECharacteristic *footChar = svc->createCharacteristic(FOOT_UUID, BLECharacteristic::PROPERTY_READ);
  footChar->setValue(String(FOOT));
  svc->start();

  // Flags + a 128-bit UUID + the name don't fit in one 31-byte packet. The app
  // finds the shoe by name, so the name goes in the main packet, where every
  // scanner sees it, and the UUID goes in the scan response.
  BLEAdvertisementData advData, scanData;
  advData.setFlags(0x06);                         // general discoverable, BLE only
  advData.setName(bleName());
  scanData.setCompleteServices(BLEUUID(SERVICE_UUID));
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->setScanResponse(true);
  // Loudest the C6 can send, so a shoe inside a case or behind a leg is still heard.
  BLEDevice::setPower(ESP_PWR_LVL_P20, ESP_BLE_PWR_TYPE_DEFAULT);
  BLEDevice::setPower(ESP_PWR_LVL_P20, ESP_BLE_PWR_TYPE_ADV);
  BLEDevice::startAdvertising();
}

bool advertising() { return BLEDevice::getAdvertising()->isAdvertising(); }

int txPowerDbm() {
  int lvl = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
  if (lvl < ESP_PWR_LVL_N15 || lvl > ESP_PWR_LVL_P20) return -128;
  return lvl == ESP_PWR_LVL_P20 ? 20 : (lvl - ESP_PWR_LVL_N0) * 3;
}

// If advertising ever stops while nobody is connected, start it again. Every
// 30 s without a connection it's also restarted from scratch, in case the radio
// says it's advertising but nothing goes out.
void keepAdvertising(uint32_t now) {
  static uint32_t lastCheck = 0, lastRefresh = 0;
  if (now - lastCheck < 2000) return;
  lastCheck = now;
  if (connected) { lastRefresh = now; return; }
  if (!advertising()) {
    BLEDevice::startAdvertising();
    Serial.println(F("# bluetooth: advertising had stopped, restarted it"));
  } else if (now - lastRefresh > 30000) {
    lastRefresh = now;
    BLEDevice::getAdvertising()->stop();
    BLEDevice::startAdvertising();
  }
}

const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_SW:       return "restart";
    case ESP_RST_PANIC:    return "CRASH (code error)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "WATCHDOG (code froze)";
    case ESP_RST_BROWNOUT: return "BROWNOUT (power dipped: power bank or wiring)";
    case ESP_RST_USB:      return "USB";
    default:               return "other";
  }
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
  Serial.println(F("# 0 app line | 1 pressure | 2 gyro | 3 steps | 4 spin | 5 state | 6 Bluetooth | 7 movements"));
  Serial.println(F("# c re-measure gyro offset and flat (shoe flat and still) | q quiet | h help"));
#if DEMO_MODE
  Serial.println(F("# DEMO MODE: sensors not read, made-up movement (DEMO_MODE in config.h)"));
#endif
  if (!pose.axisOk) Serial.println(F("# WARNING: FORWARD_AXIS points up/down when flat, using the next axis. Fix FORWARD_AXIS in config.h"));
  Serial.print(F("# parts: FSR ")); Serial.print(USE_FSR ? F("on (D0)") : F("OFF"));
  Serial.print(F(", gyro ")); Serial.print(USE_IMU ? (imu.ok ? F("on") : F("NOT FOUND (check D4 SDA / D5 SCL / VCC / GND)")) : F("OFF"));
  Serial.print(F(", steps from ")); Serial.println(stepsFromFsr ? F("pressure sensor") : F("gyro board"));
  Serial.print(F("# Bluetooth name ")); Serial.print(bleName());
  Serial.print(F(", address ")); Serial.print(BLEDevice::getAddress().toString().c_str());
  Serial.print(F(", power ")); Serial.print(txPowerDbm()); Serial.print(F(" dBm"));
  Serial.print(F(", last reset: ")); Serial.println(resetReason());
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
  pose.setFlat(imu.restAx, imu.restAy, imu.restAz);
  gravity.x = imu.restAx; gravity.y = imu.restAy; gravity.z = imu.restAz;
  Serial.print(F("# flat = up is x/y/z: "));
  Serial.print(imu.restAx, 2); Serial.print(' ');
  Serial.print(imu.restAy, 2); Serial.print(' ');
  Serial.println(imu.restAz, 2);
  if (!pose.axisOk) Serial.println(F("# WARNING: FORWARD_AXIS points up/down, fix it in config.h"));
#endif
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (c == 'h' || c == '?') printHelp();
    else if (c == 'c') calibrateGyro();
    else if ((c >= '0' && c <= '7') || c == 'q') {
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
    pose.update(now, gravity.x, gravity.y, gravity.z, imu.gx, imu.gy, imu.gz);
    peakG = fmaxf(peakG, imu.accelG());
    if (!stepsFromFsr) steps.updateAccel(now, imu.accelG());
    events.update(now, motion.state, steps, pose, gravity, imu.ax, imu.ay, imu.az, imu.gyroDps(), yawDps);
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
  pkt.pitch   = (int8_t)constrain(pose.pitch, -127.0f, 127.0f);
  pkt.roll    = (int8_t)constrain(pose.roll, -127.0f, 127.0f);
  pkt.peakG   = (uint8_t)constrain(peakG * 16, 0.0f, 255.0f);
  pkt.gyro    = (uint8_t)constrain(imu.gyroDps() / 8, 0.0f, 255.0f);
  pkt.yawRate = (int16_t)constrain(yawDps, -32000.0f, 32000.0f);
  pkt.event   = events.last;
  pkt.events  = events.count;
  pkt.pose    = pose.pose;
  pkt.air10   = (uint8_t)min(steps.lastAirMs / 10, 255);
  pkt.flags   = (imu.ok ? FLAG_IMU_OK : 0) | (steps.down ? FLAG_FOOT_DOWN : 0) | FLAG_XIAO_OK |
                (steps.touching ? FLAG_TOUCHING : 0);
  peakG = 0;
}

#if DEMO_MODE
// Every 3 seconds: STILL, WALK, RUN, SPIN, then each pose and each event once.
void makeDemoPacket(uint32_t now) {
  static uint8_t lastPhase = 0xFF;
  static uint32_t lastStep = 0;
  uint8_t phase = (now / 3000) % 12;
  bool newPhase = phase != lastPhase;
  lastPhase = phase;
  pkt.state = phase < 4 ? phase : STATE_STILL;
  uint16_t stepEvery = phase == 1 ? 1100 : 700;
  if ((phase == 1 || phase == 2) && now - lastStep > stepEvery) {
    lastStep = now; pkt.steps++; pkt.stepMs = stepEvery; pkt.air10 = 40;
  }
  if (phase == 3 && newPhase) pkt.spins++;
  pkt.spinPct = phase == 3 ? 100 : 0;
  const uint8_t poses[] = {POSE_FLAT, POSE_FLAT, POSE_FLAT, POSE_FLAT, POSE_TIPTOE, POSE_HEEL, POSE_SIDE, POSE_SKY};
  pkt.pose = phase < 8 ? poses[phase] : POSE_FLAT;
  pkt.pitch = pkt.pose == POSE_TIPTOE ? -30 : pkt.pose == POSE_HEEL ? 30 : pkt.pose == POSE_SKY ? 80 : 0;
  pkt.roll = pkt.pose == POSE_SIDE ? 30 : 0;
  if (phase >= 8 && newPhase) { pkt.event = EVENT_STOMP + (phase - 8); pkt.events++; }
  bool down = (phase == 1 || phase == 2) ? (now - lastStep < 300) : pkt.pose != POSE_SKY;
  pkt.fsr = down ? 2500 : 100;
  pkt.peakG = 16;
  pkt.yawRate = phase == 3 ? 500 : 0;
  pkt.gyro = phase == 3 ? 60 : 0;
  pkt.flags = FLAG_IMU_OK | FLAG_XIAO_OK | (down ? FLAG_FOOT_DOWN | FLAG_TOUCHING : 0);
}
#endif

// ---------------------------------------------------------------------
// Output for each debug mode. "name:value" pairs work in the Serial Plotter.
void printAppLine() {
  // FOOT,state,spins,spinPct,fsr,steps,stepMs,pitch,roll,peakG,yawRate,gyro,flags,event,events,pose,air10
  Serial.printf("%c,%u,%u,%u,%u,%u,%u,%d,%d,%u,%d,%u,%u,%u,%u,%u,%u\n", FOOT,
                pkt.state, pkt.spins, pkt.spinPct, pkt.fsr, pkt.steps, pkt.stepMs,
                pkt.pitch, pkt.roll, pkt.peakG, pkt.yawRate, pkt.gyro, pkt.flags,
                pkt.event, pkt.events, pkt.pose, pkt.air10);
}

void printDebug(uint32_t now) {
  switch (mode) {
    case MODE_APP: printAppLine(); break;
    case MODE_FSR:
      Serial.print(F("fsr:"));        Serial.print(fsr.value);
      Serial.print(F(",pressAt:"));   Serial.print((int)steps.pressAt);
      Serial.print(F(",releaseAt:")); Serial.print((int)steps.releaseAt);
      Serial.print(F(",rest:"));      Serial.print((int)steps.rest);
      Serial.print(F(",steps:"));     Serial.print(steps.count);
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
      Serial.print(connected ? F(": connected") : advertising() ? F(": advertising, waiting for the laptop") : F(": NOT advertising"));
      Serial.print(F("  address ")); Serial.print(BLEDevice::getAddress().toString().c_str());
      Serial.print(F("  power ")); Serial.print(txPowerDbm()); Serial.print(F(" dBm"));
      Serial.print(F("  sent:")); Serial.print(notifications);
      Serial.print(F("  state=")); Serial.println(stateName(pkt.state));
      break;
    case MODE_MOVES:
      Serial.print(F("pitch:"));    Serial.print(pose.pitch, 0);
      Serial.print(F(",roll:"));    Serial.print(pose.roll, 0);
      Serial.print(F(",flip:"));    Serial.print(pose.flip, 0);
      Serial.print(F(",pitchRate:"));Serial.print(pose.pitchRate, 0);
      Serial.print(F(",g10:"));     Serial.print(imu.accelG() * 10, 0);
      Serial.print(F(",gyro:"));    Serial.print(imu.gyroDps(), 0);
      Serial.print(F(",push100:")); Serial.print(events.push * 100, 0);
      Serial.print(F(",down:"));    Serial.print(steps.down ? 1 : 0);
      Serial.print(F(",touch:"));   Serial.print(steps.touching ? 1 : 0);
      Serial.print(F(",airMs:"));   Serial.print(steps.lastAirMs);
      Serial.print(F(",taps:"));    Serial.print(events.taps);
      Serial.print(F(",pushes:"));  Serial.print(events.pushes);
      Serial.print(F("  "));        Serial.print(poseName(pose.pose));
      Serial.print(' ');            Serial.println(stateName(motion.state));
      break;
  }
}

void printEvents() {
  if (steps.stepped && (mode == MODE_FSR || mode == MODE_STEPS)) {
    Serial.print(F("# step ")); Serial.print(steps.count);
    Serial.print(F(" (")); Serial.print(steps.intervalMs); Serial.println(F(" ms)"));
  }
  bool showEvents = mode == MODE_APP || mode == MODE_SPIN || mode == MODE_STATE || mode == MODE_MOVES;
  if (spin.spun && showEvents) {
    Serial.print(F("# ")); Serial.print(FOOT);
    Serial.print(F(" SPIN #")); Serial.println(spin.spins);
  }
  bool showMoves = mode == MODE_APP || mode == MODE_STATE || mode == MODE_MOVES;
  if (events.fired && showMoves) {
    Serial.print(F("# ")); Serial.print(FOOT); Serial.print(' ');
    Serial.print(eventName(events.last));
    Serial.print(F(" (")); Serial.print(events.info, 1); Serial.println(')');
  }
  if (pose.pose != lastPose) {
    if (showMoves && lastPose != 0xFF) {
      Serial.print(F("# ")); Serial.print(FOOT);
      Serial.print(F(" pose ")); Serial.print(poseName(pose.pose));
      Serial.print(F(" (pitch ")); Serial.print(pose.pitch, 0);
      Serial.print(F(", roll ")); Serial.print(pose.roll, 0); Serial.println(')');
    }
    lastPose = pose.pose;
  }
  if (steps.stepped && mode == MODE_MOVES) {
    Serial.print(F("# ")); Serial.print(FOOT);
    Serial.print(F(" landed after ")); Serial.print(steps.lastAirMs); Serial.println(F(" ms in the air"));
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
  keepAdvertising(now);
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
