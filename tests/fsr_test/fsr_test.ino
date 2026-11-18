/*
  Pressure sensor test (XIAO ESP32-C6, board XIAO_ESP32C6)
  Wiring: FSR one leg -> + rail (3.3 V), other leg -> D0, 10k resistor from D0 -> - rail.

  Open Serial Monitor or Serial Plotter at 115200.
  Expect: near 0 with nothing on it, climbing when you press, high when you stand on it.
  Stuck at 0: the FSR's + rail leg or its D0 wire is loose. Stuck near 4095: the resistor to GND is missing.
*/

void setup() {
  Serial.begin(115200);
  pinMode(D0, INPUT);
  analogReadResolution(12);   // 0..4095
}

void loop() {
  static uint16_t lo = 4095, hi = 0;
  uint16_t v = analogRead(D0);
  if (v < lo) lo = v;
  if (v > hi) hi = v;
  Serial.print("fsr:"); Serial.print(v);
  Serial.print(",lowest:"); Serial.print(lo);
  Serial.print(",highest:"); Serial.println(hi);
  delay(40);
}
