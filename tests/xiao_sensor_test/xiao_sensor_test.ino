/*
  XIAO-only sensor test (no Minima). Wiring: docs/xiao-only-rewiring.md

  Board: XIAO_ESP32C6.  Tools > USB CDC On Boot: Enabled.
  Serial Monitor: 115200 baud.

  Every 10 seconds (or when you type c and press Enter) it prints a CHECK block:
    I2C bus     which devices answer on D4 (SDA) / D5 (SCL)
    GYRO        MPU-6050 at 0x68: identity, and about 1 g while the shoe sits still
    COLOR       TCS34725 at 0x29 (fine to be missing until you add it)
    PRESSURE    FSR on D0: needs to see "released" and "pressed" once to pass
  In between, a live line twice a second:  pressure, tilt and spin numbers.
*/
#include <Wire.h>

const uint8_t FSR_PIN = D0;
const uint8_t SDA_PIN = D4;
const uint8_t SCL_PIN = D5;
const uint8_t GYRO_ADDR = 0x68;
const uint8_t COLOR_ADDR = 0x29;

const uint16_t FSR_RELEASED_MAX = 200;    // below this = foot off the sensor
const uint16_t FSR_PRESSED_MIN = 1000;    // above this = a real press

uint16_t fsrMin = 4095, fsrMax = 0;
bool gyroReady = false;
unsigned long lastLine = 0, lastCheck = 0;

bool answers(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

uint8_t readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return 0xFF;
  return Wire.read();
}

void writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

int scan(bool print) {
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (!answers(a)) continue;
    n++;
    if (print) {
      Serial.print(" 0x"); if (a < 16) Serial.print('0'); Serial.print(a, HEX);
      if (a == GYRO_ADDR) Serial.print(" (gyro)");
      else if (a == 0x69) Serial.print(" (gyro with AD0 high)");
      else if (a == COLOR_ADDR) Serial.print(" (color sensor)");
    }
  }
  return n;
}

void startGyro() {
  writeReg(GYRO_ADDR, 0x6B, 0x00);   // wake up
  delay(50);
  writeReg(GYRO_ADDR, 0x1A, 0x03);   // ~44 Hz filter
  writeReg(GYRO_ADDR, 0x1B, 0x18);   // +/-2000 deg/s
  writeReg(GYRO_ADDR, 0x1C, 0x10);   // +/-8 g
  gyroReady = true;
}

// accel in g, gyro in deg/s. false = no answer
bool readGyro(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
  Wire.beginTransmission(GYRO_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(GYRO_ADDR, (uint8_t)14) != 14) return false;
  int16_t v[7];
  for (int i = 0; i < 7; i++) { uint8_t hi = Wire.read(), lo = Wire.read(); v[i] = (int16_t)((hi << 8) | lo); }
  ax = v[0] / 4096.0f; ay = v[1] / 4096.0f; az = v[2] / 4096.0f;
  gx = v[4] / 16.4f; gy = v[5] / 16.4f; gz = v[6] / 16.4f;
  return true;
}

uint16_t readFsr() {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(FSR_PIN);
  return sum / 4;
}

void result(const char *name, bool pass, const String &detail) {
  Serial.print(pass ? "  PASS  " : "  FAIL  ");
  Serial.print(name);
  Serial.print("  ");
  Serial.println(detail);
}

void check() {
  Serial.println();
  Serial.println("==================== CHECK ====================");

  // I2C bus
  Serial.print("  I2C devices found:");
  int n = scan(true);
  if (n == 0) {
    Serial.println(" none");
    // try the wires the other way round to spot a swap
    Wire.end(); Wire.begin(SCL_PIN, SDA_PIN);
    int swapped = scan(false);
    Wire.end(); Wire.begin(SDA_PIN, SCL_PIN); Wire.setClock(100000);
    if (swapped) result("I2C bus", false, "SDA and SCL are SWAPPED. Gyro SDA must go to D4 and SCL to D5.");
    else result("I2C bus", false, "nothing answers. Check gyro VCC -> + rail (3.3V), GND -> - rail, SDA -> D4, SCL -> D5.");
  } else {
    Serial.println();
    result("I2C bus", true, String(n) + " device(s) answer on D4/D5");
  }

  // Gyro
  if (answers(GYRO_ADDR)) {
    if (!gyroReady) startGyro();
    uint8_t who = readReg(GYRO_ADDR, 0x75);
    float ax, ay, az, gx, gy, gz;
    if (readGyro(ax, ay, az, gx, gy, gz)) {
      float g = sqrtf(ax * ax + ay * ay + az * az);
      String id = "id 0x" + String(who, HEX) + (who == 0x68 ? " (MPU-6050)" : " (a compatible clone, fine)");
      result("GYRO    ", g > 0.8f && g < 1.2f,
             id + ", gravity " + String(g, 2) + " g" + (g > 0.8f && g < 1.2f ? " (good: should be about 1.00 when still)" : " (should be about 1.00 while still: keep the shoe still and re-check)"));
    } else {
      result("GYRO    ", false, "answers but readings fail. Push the SDA/SCL wires in firmly.");
    }
  } else if (answers(0x69)) {
    result("GYRO    ", false, "found at 0x69: its AD0 pin is connected. Leave AD0 empty.");
  } else {
    gyroReady = false;
    result("GYRO    ", false, "not found at 0x68. Check its VCC, GND, SDA (D4) and SCL (D5) wires.");
  }

  // Color sensor (optional for now)
  if (answers(COLOR_ADDR)) {
    uint8_t id = readReg(COLOR_ADDR, 0x80 | 0x12);   // command bit + ID register
    result("COLOR   ", true, "TCS34725 found, id 0x" + String(id, HEX));
  } else {
    Serial.println("  ----  COLOR     not connected (that's fine until you add the TCS34725)");
  }

  // Pressure sensor
  uint16_t f = readFsr();
  bool sawReleased = fsrMin <= FSR_RELEASED_MAX, sawPressed = fsrMax >= FSR_PRESSED_MIN;
  String d = "now " + String(f) + " (lowest " + String(fsrMin) + ", highest " + String(fsrMax) + "). ";
  if (fsrMin >= 4000) d += "Always at the top: the 10k resistor to the - rail is missing, or D0 touches 3.3V directly.";
  else if (!sawPressed) d += "Press the sensor hard with your thumb to pass. If the number never rises, check the FSR wires and that its row connects to D0.";
  else if (!sawReleased) d += "Let go of the sensor to pass.";
  else d += "Released and pressed both seen.";
  result("PRESSURE", sawReleased && sawPressed && fsrMin < 4000, d);
  Serial.println("================================================");
  Serial.println("  Type c + Enter to check again. Live: pressure 0-4095 | gravity g | spin deg/s");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  delay(300);
  Serial.println();
  Serial.println("# XIAO sensor test. FSR on D0, gyro on D4 (SDA) / D5 (SCL).");
  analogReadResolution(12);
  pinMode(FSR_PIN, INPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(100);
  check();
  lastCheck = millis();
}

void loop() {
  uint16_t f = readFsr();
  if (f < fsrMin) fsrMin = f;
  if (f > fsrMax) fsrMax = f;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'C') { check(); lastCheck = millis(); }
  }

  if (millis() - lastLine >= 500) {
    lastLine = millis();
    Serial.print("pressure ");
    Serial.print(f);
    Serial.print(f >= FSR_PRESSED_MIN ? " (pressed) " : f <= FSR_RELEASED_MAX ? " (released)" : " (light)   ");
    float ax, ay, az, gx, gy, gz;
    if (gyroReady && readGyro(ax, ay, az, gx, gy, gz)) {
      Serial.print(" | gravity "); Serial.print(sqrtf(ax * ax + ay * ay + az * az), 2);
      Serial.print(" g  x "); Serial.print(ax, 2); Serial.print(" y "); Serial.print(ay, 2); Serial.print(" z "); Serial.print(az, 2);
      Serial.print(" | spin "); Serial.print(sqrtf(gx * gx + gy * gy + gz * gz), 0); Serial.println(" deg/s");
    } else {
      Serial.println(" | gyro: no answer");
    }
  }

  if (millis() - lastCheck >= 10000) { check(); lastCheck = millis(); }
  delay(20);
}
