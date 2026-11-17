#pragma once
// =====================================================================
//  Settings for one shoe. Change these, then upload again.
// =====================================================================

#define FOOT 'L'               // <<< 'L' for the left shoe, 'R' for the right shoe

// ---------- Turn parts on/off (for debugging one part at a time) ----------
// Set a part to 0 to run the shoe without it. Everything else keeps working.
#define USE_FSR   1            // pressure sensor in the sole (A0)
#define USE_IMU   1            // MPU-6050 gyro on the breadboard (SDA/SCL)
#define USE_XIAO  1            // send to the XIAO (Bluetooth) over SDA/SCL

// Where steps come from:
//   1 = pressure sensor (a step = the foot pressing down)
//   0 = gyro board (a step = a hit on the ground)
// If USE_FSR is 0, the gyro board is used no matter what this says.
#define STEPS_FROM_FSR 1

// ---------- Timing ----------
#define SAMPLE_HZ   100        // how often the sensors are read
#define SEND_HZ     25         // how often numbers go to USB and the XIAO

// ---------- Pins and addresses ----------
#define FSR_PIN     A0
#define MPU_ADDR    0x68       // 0x69 if the GY-521 AD0 pin is wired to VCC
#define XIAO_ADDR   0x08       // must match xiao_radio.ino

// ---------- Pressure sensor (0..4095) ----------
// The shoe learns how hard this person presses. "Foot down" is when pressure
// goes above FSR_PRESS_FRAC of that, and "foot up" is below FSR_RELEASE_FRAC.
#define FSR_MIN_PRESS     150  // anything below this is never "foot down" (noise)
#define FSR_PRESS_FRAC    0.40f
#define FSR_RELEASE_FRAC  0.20f

// ---------- Steps from the gyro board (only when STEPS_FROM_FSR is 0) ----------
#define ACCEL_STEP_G      1.6f // a hit harder than this (in g) is a step
#define ACCEL_RESET_G     1.15f// must drop back under this before the next step

#define STEP_MIN_MS       250  // two steps closer than this count as one
#define STEP_MAX_GAP_MS   2000 // a pause longer than this starts fresh

// ---------- Walk / run / still ----------
// One shoe only sees its own steps, so these are times between steps of the
// SAME foot. Walking is about 1000-1200 ms, running about 600-750 ms.
#define RUN_STEP_MS       800  // faster than this = running
#define RUN_HYST_MS       100  // must slow to RUN_STEP_MS + this to go back to walking
#define RUN_HOLD_MS       1100 // no step for this long = not running anymore
#define STILL_MS          2000 // no step for this long = still (sound pauses)
// Walking needs at least 2 steps of this foot, no more than STEP_MAX_GAP_MS apart.
// Tilting or shaking the shoe without pressing the sole never counts.

// ---------- 360 spin ----------
#define SPIN_RATE_DPS     120.0f // turning faster than this counts toward a spin
#define SPIN_DEG          330.0f // a spin is this many degrees (a bit under 360 to be forgiving)
#define SPIN_WINDOW_MS    3000   // the whole spin has to happen within this time
#define SPIN_GAP_MS       400    // slowing down for longer than this cancels the spin
#define SPIN_SHOW_MS      700    // after a spin, report SPIN for this long
