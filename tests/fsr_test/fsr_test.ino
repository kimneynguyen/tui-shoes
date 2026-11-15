/*
  Pressure sensor test (Minima only, nothing else needed)
  Wiring: FSR one leg -> 5V, other leg -> A0, resistor from A0 -> GND.

  Open Serial Monitor or Serial Plotter at 115200.
  Expect: near 0 with nothing on it, climbing when you press, high when you stand on it.
  Stuck at 0: the 5V leg or A0 wire is loose. Stuck near 4095: the resistor to GND is missing.
*/

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);   // 0..4095
}

void loop() {
  static uint16_t lo = 4095, hi = 0;
  uint16_t v = analogRead(A0);
  if (v < lo) lo = v;
  if (v > hi) hi = v;
  Serial.print("fsr:"); Serial.print(v);
  Serial.print(",lowest:"); Serial.print(lo);
  Serial.print(",highest:"); Serial.println(hi);
  delay(40);
}
