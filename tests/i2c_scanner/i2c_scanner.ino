/*
  SDA/SCL wire check (Minima with the gyro and the XIAO plugged in)

  Open Serial Monitor at 115200. Every 2 seconds it lists who answers:
     0x68  the gyro board (0x69 if its AD0 pin goes to VCC)
     0x08  the XIAO (only if xiao_radio is uploaded to it and DEMO_MODE is 0)
  Missing gyro: check its SDA/SCL/VCC/GND.  Missing XIAO: check D4/D5, 5V, GND.
*/
#include <Wire.h>

// Same as minima_foot: the R4's 5 V pull-ups would reach the 3.3 V XIAO, so turn them off.
void i2cPullupsOff() {
  R_IOPORT_PinCfg(&g_ioport_ctrl, g_pin_cfg[WIRE_SDA_PIN].pin,
                  (uint32_t)(IOPORT_CFG_DRIVE_MID | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_IIC));
  R_IOPORT_PinCfg(&g_ioport_ctrl, g_pin_cfg[WIRE_SCL_PIN].pin,
                  (uint32_t)(IOPORT_CFG_DRIVE_MID | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_IIC));
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000);
  i2cPullupsOff();
}

void loop() {
  Serial.print("# found:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(" 0x"); if (a < 16) Serial.print('0'); Serial.print(a, HEX);
      if (a == 0x68 || a == 0x69) Serial.print(" (gyro)");
      if (a == 0x08) Serial.print(" (XIAO)");
      n++;
    }
  }
  Serial.println(n ? "" : " nothing. Check the SDA/SCL wires and power.");
  delay(2000);
}
