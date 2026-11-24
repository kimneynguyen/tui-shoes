# TUI shoes

A pair of shoes to build a new world. 

## Pictures

**Testing User Interfaces** Both shoes are connected via bluetooth, feeding live data to user interfaces that were custom built for each phase of testing. First attempt at testing as many possible movements with the components inherited from the previous "Music Shoes" lab - Gyro, Xiao, and FSR. 

**Testing all movements with UI dashboard aid**

<img width="998" height="796" alt="Screenshot 2026-11-24 at 11 24 13 AM" src="https://github.com/user-attachments/assets/ff8e45c4-02b5-4f13-a252-b0623c2126fa" />

<img width="1000" height="797" alt="Screenshot 2026-11-24 at 11 23 08 AM" src="https://github.com/user-attachments/assets/ff811eb5-f844-46c7-8bf2-fe61adf3febc" />


**Tinkercad wiring** for one shoe's breadboard: the XIAO (left), the gyro (right, bottom half), and the FSR with its 220 ohm resistor (top right).

![Tinkercad breadboard wiring](docs/tinkercad-circuit.png)

**The real left-shoe build** the XIAO ESP32-C6 (small black board) and the MPU-6050 gyro (purple board, red light) on the breadboard.

<img width="1023" height="765" alt="Screenshot 2026-11-24 at 11 25 48 AM" src="https://github.com/user-attachments/assets/bb7f6a92-116d-413a-b737-8733700bb460" />


## What's in this folder

| Folder | What it is | Goes on |
|---|---|---|
| `xiao_radio/` | Sends the numbers to the laptop by Bluetooth | Each XIAO ESP32-C6 |
| `sampler/` | The movement detection app | The laptop (open in Chrome) |
| `tinkercad/` | The Tinkercad practice model | Tinkercad (two Unos) |
| `gyro/` | Reads input on four axis of the gyro (axis, tilt, spin, airtime) | Each MPU-6050 gyro |

Before you upload, set `FOOT` to `L` for the left shoe or `R` for the right shoe. Do it in the config file of the XIAO_RADIO code.

## Parts (for both shoes)

| Part | How many |
|---|---|
| Breadboard | 2 |
| XIAO ESP32-C6 (pre-soldered) | 2 (+1 spare) |
| GY-521 MPU-6050 gyro (pre-soldered) | 2 (+1 spare) |
| FSR pressure sensor | 2 (+1 spare) |
| 10K ohm resistor (red red brown) | 2 (one per shoe, for the FSR) |
| 220 ohm capacitor | 2 (one per shoe, for the FSR) |
| Jumper wires | a bunch |
| USB power bank + USB-C cable | 2 |

No soldering. Everything pushes into the breadboard.

## Wiring (same on both shoes)

**Pressure sensor (FSR), in the sole, under the ball of the foot**
- One leg to the XIAO 3V3 pin. Never 5V.
- Other leg to D0.
- 10k resistor from D0 to GND.

**Gyro (MPU-6050), on the breadboard**
- VCC to 3V3
- GND to GND
- SDA to D4
- SCL to D5

**Power:** power bank → Minima's USB-C port. That powers everything.


## Connecting to the sounding board app

1. Open the sounding board app (see below). Press **Start music**. Move the sliders. It works with no shoes!
2. Plug in one Minima. Upload. Open Serial Monitor (115200). You should see numbers like `L,1830,2.41,-0.87,12`. Press the FSR: the 2nd number goes up.
3. In the music app, click **USB test** and pick the Minima. The bars move when you press.
4. Add the XIAO. In the app, click **Connect Left shoe** and pick `MusicShoe-L`.
5. Build the right shoe the same way with `R`.
6. Put them on. Stand flat, click **Set: standing flat**. Lift your foot, click **Set: foot lifted**. Play!

## Opening the sounding board app

Bluetooth only works if you open it this way:
```
cd sampler
python3 -m http.server 8000
```
Then go to `http://localhost:8000` in Chrome.


## If something goes wrong

- coming...
