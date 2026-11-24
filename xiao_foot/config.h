#pragma once
// =====================================================================
//  Settings for one shoe. Change these, then upload again.
// =====================================================================

#define FOOT 'L'               // <<< 'L' for the left shoe, 'R' for the right shoe

// ---------- Turn parts on/off (for debugging one part at a time) ----------
// Set a part to 0 to run the shoe without it. Everything else keeps working.
#define USE_FSR   1            // pressure sensor in the sole (D0)
#define USE_IMU   1            // MPU-6050 gyro on the breadboard (D4 SDA / D5 SCL)

// 1 = no sensors needed: every 3 seconds makes up still, walk, run, spin, then
// tiptoe, heel, side, sky, then stomp, toe tap, kick, shuffle,
// to test Bluetooth and the laptop app on their own.
#define DEMO_MODE 0

// Where steps come from:
//   1 = pressure sensor (a step = the foot pressing down)
//   0 = gyro board (a step = a hit on the ground)
// If USE_FSR is 0, the gyro board is used no matter what this says.
#define STEPS_FROM_FSR 1

// ---------- Timing ----------
#define SAMPLE_HZ   100        // how often the sensors are read
#define SEND_HZ     25         // how often numbers go to USB and Bluetooth

// ---------- Pins and addresses ----------
// The labels printed on the XIAO: D0 = A0, D4 = SDA, D5 = SCL.
#define FSR_PIN     D0
#define SDA_PIN     D4
#define SCL_PIN     D5
#define MPU_ADDR    0x68       // 0x69 if the GY-521 AD0 pin is wired to VCC

// ---------- Pressure sensor (0..4095, FSR from 3.3 V with 10k to GND) ----------
// The shoe learns how hard this person presses (peak) and what the sensor reads
// with the foot up (rest). "Foot down" is above FSR_PRESS_FRAC of the way from
// rest to peak, and "foot up" is below FSR_RELEASE_FRAC. Key 1 shows both lines.
#define FSR_MIN_PRESS     150  // anything below this is never "foot down" (noise)
#define FSR_PRESS_FRAC    0.40f
#define FSR_RELEASE_FRAC  0.20f
#define FSR_MIN_SPAN      400  // rest to peak is taken as at least this, so noise is never a step
#define FSR_PEAK_HALF_MS  3000 // one hard squeeze is half forgotten after this (lighter steps count again)
#define FSR_REST_RISE_MS  2000 // how fast the rest level follows a sensor that stays pressed with the foot up

// ---------- Steps from the gyro board (only when STEPS_FROM_FSR is 0) ----------
#define ACCEL_STEP_G      1.6f // a hit harder than this (in g) is a step
#define ACCEL_RESET_G     1.15f// must drop back under this before the next step

// ---------- Air time (accelerometer only, not the pressure sensor) ----------
#define AIR_FREEFALL_G    0.40f  // total acceleration under this (g) = weightless = in the air
#define AIR_MIN_MS        40     // ...for at least this long (shorter dips are bumps)
#define AIR_LAND_G        1.20f  // a hit over this (g) ends the time in the air
#define AIR_MAX_MS        2000   // weightless longer than this is a dropped shoe, not air time

#define STEP_MIN_MS       250  // two steps closer than this count as one
#define STEP_MAX_GAP_MS   2000 // a pause longer than this starts fresh

// ---------- Walk / run / still ----------
// One shoe only sees its own steps, so these are times between steps of the
// SAME foot. Walking is about 1000-1200 ms, running about 600-750 ms.
#define RUN_STEP_MS       800  // faster than this = running (the shoe's own guess; the app decides by RUN_SPM, see the step-rate graph)
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

// =====================================================================
//  Movement detection (one foot). Moves that need both feet (jump, hop,
//  skip, lunge, dancer's pose, feet to the sky) are tuned in the app:
//  the TUNING block at the top of sampler/movements.js.
//  Every number here is a first guess: none of it is tested on real feet yet.
//  Watch debug key 7 while you move to see the numbers.
// =====================================================================

// ---------- How the gyro board sits in the shoe ----------
// Which axis printed on the GY-521 points to the toes: 0 = X, 1 = Y, 2 = Z,
// and FORWARD_SIGN 1 if the arrow points to the toes, -1 if it points to the heel.
// Check with key 7: lift your toes and "pitch" should go UP (positive).
// If "roll" moves instead, try the other axis. If pitch goes negative, flip the sign.
#define FORWARD_AXIS      0
#define FORWARD_SIGN      1
// "Flat" is saved when the gyro offset is measured: at power on, and with key c.
// Have the shoe flat on the floor (or stand normally) at those moments.

// ---------- Foot tilt and feet to the sky (pose) ----------
#define TILT_DEG          20     // toes down, heel down or rolled sideways more than this = tilted
#define TILT_EXIT_DEG     12     // back under this = flat again
#define SKY_PITCH_DEG     60     // toes up more than this = pointing at the sky
#define SKY_FLIP_DEG      120    // or turned over more than this (sole facing the ceiling)
#define POSE_HOLD_MS      250    // a new pose has to last this long before it counts

// ---------- Stomp ----------
#define STOMP_G           3.5f   // a hit harder than this (g) just after the foot presses down = stomp
#define STOMP_WINDOW_MS   80     // how long after the press to look for the hit
// Never while this foot is running.

// ---------- Toe tap ----------
#define TAP_RATE_DPS      120    // toes come down faster than this (deg/s)...
#define TAP_G             1.4f   // ...then hit the floor harder than this (g), but softer than STOMP_G = one tap
#define TAP_MAX_LIFT_MS   300    // sole off the ground longer than this before the hit = a step, not a tap
#define TAP_COUNT         2      // this many taps in a row = TOE TAP (then every further tap)
#define TAP_WINDOW_MS     1200   // taps further apart than this start a new row

// ---------- Karate kick ----------
#define KICK_GYRO_DPS     700    // foot in the air rotating faster than this (deg/s)...
#define KICK_G            2.5f   // ...and accelerating harder than this (g) = kick
#define KICK_MIN_AIR_MS   120    // foot has to be off the ground at least this long
// One kick per lift, never while this foot is running.

// ---------- Shuffle / slide ----------
#define SHUFFLE_G         0.35f  // a push along the floor harder than this (g), sole touching...
#define SHUFFLE_PUSHES    3      // ...this many pushes in a row = SHUFFLE
#define SHUFFLE_GAP_MS    600    // pushes further apart than this start over
#define SHUFFLE_NO_STEP_MS 800   // no step for this long before (sliding, not walking)
#define SHUFFLE_REPEAT_MS 400    // while it goes on, report it at most this often
