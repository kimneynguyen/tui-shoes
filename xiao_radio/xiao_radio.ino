/*
  Sound Effect Shoes : XIAO radio sketch (one per shoe)
  -----------------------------------------------------
  Board: Seeed XIAO ESP32-C6 (IDE board "XIAO_ESP32C6"), esp32 board package 3.x.
  Job:   listen to the Minima on the I2C wires (SDA/SCL, shared with the gyro),
         send each 20-byte FootPacket to the laptop over Bluetooth (BLE).

  Wiring:
     Minima 5V  -> XIAO 5V
     Minima GND -> XIAO GND
     XIAO D4    -> SCL  (same wire as the gyro's SCL)
     XIAO D5    -> SDA  (same wire as the gyro's SDA)
     No resistors: the gyro board already pulls SDA/SCL up to 3.3 V.
     Note: on this build D4 is SCL and D5 is SDA (swapped from the XIAO's printed
     defaults) so the wires on the breadboard don't cross. The code below matches.

  DEBUGGING (Serial Monitor, 115200):
     Prints a status line every second: Bluetooth, Minima link, and the last packet.
     Type  v  + Enter to print every packet, and  v  again to stop.
     Set DEMO_MODE to 1 to test Bluetooth with NO Minima: the XIAO makes up
     walk / run / spin / still on its own, so you can check the laptop app alone.

  BEFORE UPLOADING: set FOOT below to match the Minima on the same shoe.
  Upload this to the XIAO BEFORE plugging it into the breadboard.
*/

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "soc/pcr_struct.h"   // for the I2C clock fix in setup()
#include "packet.h"

#define FOOT "L"         // <<< "L" or "R", must match the Minima on this shoe
#define DEMO_MODE 0      // 1 = no Minima needed, send made-up movement

#define XIAO_ADDR 0x08   // must match minima_foot/config.h
#define PIN_SDA   D5
#define PIN_SCL   D4

// Must match the app (sampler/index.html)
#define SERVICE_UUID "7b1e0001-5d2c-4c3b-9a41-50a1c5717e00"
#define DATA_UUID    "7b1e0002-5d2c-4c3b-9a41-50a1c5717e00"

const char *STATE_NAMES[] = {"STILL", "WALK", "RUN", "SPIN"};

BLECharacteristic *dataChar = nullptr;
volatile bool connected = false;
FootPacket data = {};
volatile FootPacket incoming = {};
volatile bool fresh = false;
volatile uint32_t lastMsg = 0, packets = 0, badPackets = 0;
uint32_t lastNotify = 0, lastDbg = 0, packetsAtLastDbg = 0;
bool verbose = false;

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) override { connected = true; }
  void onDisconnect(BLEServer *s) override {
    connected = false;
    BLEDevice::startAdvertising();   // let the laptop reconnect
  }
};

// Called when the Minima writes to us
void onI2CReceive(int n) {
  if (n == (int)sizeof(FootPacket)) {
    uint8_t *p = (uint8_t *)&incoming;
    for (int i = 0; i < n; i++) p[i] = Wire.read();
    fresh = true;
    lastMsg = millis();
    packets++;
  } else {
    while (Wire.available()) Wire.read();   // wrong size, throw it away
    badPackets++;
  }
}

#if DEMO_MODE
// Cycles STILL -> WALK -> RUN -> SPIN every 4 seconds.
void makeDemoPacket(uint32_t now) {
  static uint8_t lastPhase = 0xFF;
  static uint32_t lastStep = 0;
  uint8_t phase = (now / 4000) % 4;
  data.state = phase;
  uint16_t stepEvery = phase == 1 ? 1100 : 700;
  if ((phase == 1 || phase == 2) && now - lastStep > stepEvery) {
    lastStep = now; data.steps++; data.stepMs = stepEvery;
  }
  if (phase == 3 && lastPhase != 3) data.spins++;
  lastPhase = phase;
  data.spinPct = phase == 3 ? 100 : 0;
  data.fsr = (phase == 1 || phase == 2) ? ((now - lastStep < 300) ? 2500 : 100) : (phase == 0 ? 1800 : 1200);
  data.ax = 0; data.ay = 0; data.az = 100;
  data.yawRate = phase == 3 ? 500 : 0;
  data.gyro = phase == 0 ? 2 : (phase == 3 ? 500 : 150);
  data.flags = 0x01;
  lastMsg = now;
}
#endif

void printPacket(const FootPacket &d) {
  Serial.printf("%s,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%u,%u\n", FOOT,
                d.state, d.spins, d.spinPct, d.fsr, d.steps, d.stepMs,
                d.ax, d.ay, d.az, d.yawRate, d.gyro, d.flags);
}

void setup() {
  Serial.begin(115200);       // USB debug
  Serial.setTxTimeoutMs(0);   // never wait on USB: if no Serial Monitor is open, printing would stall Bluetooth

#if !DEMO_MODE
  Wire.onReceive(onI2CReceive);
  Wire.begin((uint8_t)XIAO_ADDR, PIN_SDA, PIN_SCL, 100000);   // listen, don't drive the clock
  // Fix for the esp32 3.3.x library on the C6: it leaves the I2C clock switched off
  // in listen mode, so the XIAO never answers. This line switches it on.
  PCR.i2c_sclk_conf.i2c_sclk_en = 1;
#endif

  String name = String("MusicShoe-") + FOOT;
  BLEDevice::init(name.c_str());
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
  Serial.println("# " + name + (DEMO_MODE ? " DEMO MODE (no Minima)" : " listening on I2C 0x08") + ", advertising");
}

void loop() {
  uint32_t now = millis();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'v') { verbose = !verbose; Serial.println(verbose ? "# printing every packet" : "# packet printing off"); }
  }

#if DEMO_MODE
  makeDemoPacket(now);
  bool gotNew = now - lastNotify >= 40;
#else
  bool gotNew = false;
  if (fresh) {
    noInterrupts();
    memcpy(&data, (const void *)&incoming, sizeof(data));
    fresh = false;
    interrupts();
    gotNew = true;
    if (verbose) printPacket(data);
  }
#endif

  // pass each new packet on, at most 40 times a second
  if (connected && gotNew && now - lastNotify >= 25) {
    lastNotify = now;
    dataChar->setValue((uint8_t *)&data, sizeof(data));
    dataChar->notify();
  }

  // once a second, a status line for debugging
  if (now - lastDbg > 1000 && Serial.availableForWrite() >= 128) {   // skip if nobody is reading USB
    uint32_t rate = packets - packetsAtLastDbg;
    packetsAtLastDbg = packets;
    lastDbg = now;
    Serial.printf("# %s  ble:%s  minima:%s (%lu/s, %lu bad)  state=%s spins=%u steps=%u fsr=%u\n",
                  FOOT, connected ? "connected" : "waiting",
                  (now - lastMsg < 500) ? "ok" : "NO DATA (check D4/D5 wires)",
                  (unsigned long)rate, (unsigned long)badPackets,
                  data.state < 4 ? STATE_NAMES[data.state] : "?", data.spins, data.steps, data.fsr);
  }
}
