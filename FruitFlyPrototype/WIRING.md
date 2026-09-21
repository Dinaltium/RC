# Wiring — FruitFly 2-motor build

Everything below matches `RobotPrototype/config.h`. Common ground for
battery, drivers, sensors and ESP32 is assumed throughout.

## GPIO map

| GPIO | Function | Notes |
|---|---|---|
| 16, 17 | Motor FL IN1/IN2 (DRV8833 #1, bridges paralleled) | see `SWAP_LEFT_RIGHT` below |
| 18, 19 | Motor FR IN1/IN2 (DRV8833 #2, bridges paralleled) | |
| 23 | DRV8833 `EEP` enable (both modules) | driven HIGH at boot |
| 26 | HC-SR04 `TRIG` | only with `FRONT_RANGE_HCSR04` |
| 27 | HC-SR04 `ECHO` (via divider) **or** LD2420 `OT2` | one or the other |
| 21 | I²C SDA | all VL53L0X **and** the OLED |
| 22 | I²C SCL | all VL53L0X **and** the OLED |
| 25 | VL53L0X `XSHUT`, forward-facing | only with `FRONT_RANGE_VL53L0X` |
| 32 | VL53L0X `XSHUT`, cliff front | |
| 33 | VL53L0X `XSHUT`, cliff rear | |
| 34 | Front bump endstop | input-only, needs external pull-up |
| 35 | Rear bump endstop | input-only, needs external pull-up |

Free for later: 0 (boot strap, avoid), 2 (LED), 4, 5, 12–15 (JTAG/strapping, use with care), 36/39 (input-only ADC).

**Left and right swapped?** If `LEFT` made the chassis turn right (the two
driver modules were wired mirrored), leave `SWAP_LEFT_RIGHT = true` in
`config.h` — the firmware exchanges the two pin sets in software. Set it to
`false` only if you re-wire the harness so module #1 really drives the
left wheel. Forward/backward are unaffected either way.

## 1. HC-SR04 ultrasonic (default front range)

```text
HC-SR04         ESP32
  VCC  ───────  5 V  (VIN / USB 5 V; NOT 3.3 V, the sensor is unreliable below 5 V)
  GND  ───────  GND
  TRIG ───────  GPIO 26
  ECHO ──[1 kΩ]──┬──  GPIO 27
                 │
               [2 kΩ]
                 │
                GND
```

ECHO is a 5 V output; the 1 k / 2 k divider brings it to 3.3 V. Without it
you will eventually damage the GPIO. Mount at bumper height, facing forward,
nothing within ~5 cm in front of the transducers (that is its blind zone,
the front bumper covers that).

Firmware: `#define FRONT_RANGE_HCSR04` (already the default). Stop at
≤ 30 cm, resume at ≥ 35 cm.

## 2. Limit-switch bumpers (3D-printer endstops) — two switches

Use the **mechanical microswitch** endstops (the ones with a metal lever),
not the optical ones.

### Which tabs

A bare microswitch has three tabs: `C` (common), `NC` (normally closed,
connected to C while the lever is *not* pressed) and `NO` (normally open).
Use `C` and `NC`. If unsure, a multimeter in continuity mode beeps between
`C` and `NC` with the lever at rest.

Many printer endstops come on a small PCB with a 3-pin plug labelled
`S / - / +` (signal, ground, supply). Those boards already contain the
pull-up (and usually an LED) and output **HIGH when pressed** — exactly what
the firmware expects. For those:

```text
endstop PCB     ESP32
  +   ───────   3.3 V      (not 5 V: the signal must stay at 3.3 V logic)
  -   ───────   GND
  S   ───────   GPIO 34 (front)   or   GPIO 35 (rear)
```

No external resistor is needed with the PCB version. Check with a meter
that `S` is ~0 V at rest and ~3.3 V pressed before plugging into the ESP32;
a few boards are wired the other way round, in which case use the bare
switch tabs as below instead.

### (a) One switch front, one switch rear — the normal setup

Wire **normally closed** to ground with an external pull-up, so a broken
wire or unplugged connector reads as "pressed" and the robot refuses to
drive into it. GPIO 34 and 35 are input-only and have **no internal
pull-ups**, so the resistor is required — one per switch.

```text
             3.3 V                               3.3 V
               │                                   │
            [10 kΩ]                             [10 kΩ]
               │                                   │
GPIO 34 ───────┼──────── C  (front switch)   GPIO 35 ───────┼──────── C  (rear switch)
               │           \                                │           \
               │            \ lever at rest = closed        │            \ lever at rest = closed
               │             \                              │             \
              GND ───────── NC                             GND ───────── NC
```

Resting level LOW (closed to ground), pressed = HIGH. Any value from
4.7 kΩ to 22 kΩ works for the pull-up; 10 kΩ is the usual pick. Two 1 kΩ in
series (2 kΩ) is **too low** here — it still works electrically but wastes
1.6 mA per switch and gives less noise margin; keep the 1 k/2 k resistors
for the ultrasonic divider. Put each resistor close to the ESP32.

### (b) Two switches on the same bumper bar

When one bumper bar is wide enough to need a switch at each end, put the
two switches in **series** on the same GPIO. Any one of them opening trips
the input; the wiring uses one pull-up for the pair.

```text
                    3.3 V
                      │
                   [10 kΩ]
                      │
GPIO 34 ──────────────┼──── C  switch A (left end)
                      │      NC ────── C  switch B (right end)
                      │                  NC ────── GND
```

The same pattern applies to the rear bumper on GPIO 35.

### Mechanics

A bumper bar (3D-printed or a strip of plastic) hinged so that a push
anywhere along it presses the lever. Mount it 5–10 mm proud of the chassis
so it trips *before* the chassis hits.

### Enabling it in firmware

The bump inputs are compiled out until the switches exist, because a
floating GPIO 34/35 reads HIGH = "pressed" and would block the robot. Once
wired:

1. In `RobotPrototype/config.h` uncomment `#define ENABLE_BUMP`.
2. Re-flash (`arduino-cli compile ... && arduino-cli upload ...`, see §7).
3. Serial at boot: `[BUMP] Initialized front=clear rear=clear`. Press each
   lever: `[BUMP] front pressed` / `[BUMP] front released`. The `[STATUS]`
   line shows `bump=F-`, `bump=-R` or `bump=FR`.
4. Web UI (`http://192.168.4.1/`): the *Bumpers F/R* row turns red and a
   banner appears; `/api/status` reports `bump_front` / `bump_rear`. The fly
   dashboard shows it under *Robot → bumpers front / rear*.

Behaviour: front pressed → FORWARD, LEFT, RIGHT and both rotations are
blocked, BACKWARD allowed. Rear pressed → mirror. Clears automatically when
the switch releases (not latched). The fly host treats a front bump as a
trigger to back up and turn away.

## 3. VL53L0X ToF — cliff sensors (front and rear, looking down)

Every VL53L0X wakes up at I²C address 0x29. The firmware holds each one in
reset via `XSHUT`, wakes them one by one and re-addresses them (0x31 front
cliff, 0x32 rear cliff, 0x30 forward range), so the XSHUT wires are **not
optional** when more than one sensor is on the bus.

```text
VL53L0X (each)      ESP32
  VIN  ───────────  3.3 V   (modules with a regulator also accept 5 V; either is fine)
  GND  ───────────  GND
  SDA  ───────────  GPIO 21   ← shared by all
  SCL  ───────────  GPIO 22   ← shared by all
  XSHUT ──────────  GPIO 32   (front cliff)
                    GPIO 33   (rear cliff)
                    GPIO 25   (forward range, if used)
  GPIO1 (interrupt) not connected
```

Most breakouts already carry 10 kΩ pull-ups on SDA/SCL. If you use bare
modules without them, add 4.7 kΩ from SDA and SCL to 3.3 V once.

Placement for cliff detection:

* Point straight down, about **2 cm ahead of the front wheel contact line**
  (and behind the rear one). Further ahead = earlier warning but more
  false trips on floor texture.
* Mount height 40–80 mm above the floor. Measure the actual reading on flat
  floor from the serial `[TOF]`/`[STATUS]` lines and set
  `CLIFF_FLOOR_MM` to it; `CLIFF_MARGIN_MM = 40` means a drop of more than
  4 cm trips it. `CLIFF_CONFIRM_READS = 2` (80 ms) filters single bad reads.
* Keep the sensor's window clear of the chassis edge; VL53L0X has a ~25°
  cone.

Behaviour: a drop **latches**. Front drop → forward and rotations blocked,
BACKWARD allowed to retreat; rear → mirror. Press **Clear E-Stop** in the
web UI (or send `CLEAR_EMERGENCY`) after backing to safety to release it.
A cliff sensor that fails to initialise is logged loudly; set
`CLIFF_FAIL_SAFE = true` to refuse motion on that side instead.

## 4. VL53L0X instead of the ultrasonic (forward range)

Yes, and it is a better sensor for this job: narrower beam, real millimetre
readings, no 5 V or divider, immune to soft/angled surfaces that swallow
ultrasound. Limits to know:

| | HC-SR04 | VL53L0X |
|---|---|---|
| range | 2 cm – ~4 m | 3 cm – ~1.2 m indoors (2 m on white walls, much less in sunlight) |
| beam | ~15° cone, catches wide obstacles | ~25° but effectively a spot: can miss a thin table leg off-centre |
| bad at | soft cloth, angled surfaces, other ultrasonics nearby | direct sunlight, black/mirror surfaces, dirty window |
| supply | 5 V + divider | 3.3 V, I²C |

For a 30 cm stop threshold the VL53L0X is well inside its comfortable
range. Wire it like the cliff sensors with `XSHUT` on **GPIO 25**, then in
`config.h`:

```cpp
// #define FRONT_RANGE_HCSR04
// #define FRONT_RANGE_LD2420
#define FRONT_RANGE_VL53L0X
```

That frees GPIO 26/27. If you want the best of both, keep the ultrasonic
for wide coverage and add the forward ToF later as a second input — that is
a small firmware change (min of the two distances).

## 5. OLED status display (SSD1306 128×64, I²C)

Optional, but handy: the chassis tells you what it is doing without a
serial cable or the dashboard.

```text
SSD1306 module      ESP32
  VCC  ───────────  3.3 V   (most modules accept 3.3–5 V; 3.3 V is safest for the I²C lines)
  GND  ───────────  GND
  SDA  ───────────  GPIO 21   ← shared with the VL53L0X sensors
  SCL  ───────────  GPIO 22   ← shared with the VL53L0X sensors
```

I²C address 0x3C on nearly all 128×64 modules; a few use 0x3D — change
`OLED_ADDR` in `config.h` if the display stays blank and the serial log says
`[OLED] Nothing at 0x3C`. It shares the bus with the ToF sensors without
conflict (they live at 0x29–0x32).

Firmware: `#define ENABLE_OLED` is **on by default**. At boot the firmware
probes the address; if nothing answers it logs once and carries on, so the
build works with or without the display plugged in. When present, the
display refreshes every 200 ms with:

* row 1, large: the current command (`FORWARD`, `ROTATE_LEFT`, …) or
  `E-STOP` when the latch is set;
* speed (PWM) and the command source (`PHONE_WIFI`, `SERIAL_DEBUG`, `NONE`);
* range in cm and `OBST` when the obstacle latch is active;
* bumper and cliff flags (`bump F-  cliff -R` style);
* the AP address (`192.168.4.1`) and how many clients are connected;
* uptime in seconds.

Compiling needs two libraries (already installed on the laptop):

```bash
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library"
```

## 6. ESP32-CAM (AI Thinker) as the robot camera

The camera board is **not wired to the main ESP32 at all**. It joins the
robot's Wi-Fi AP as a client with the fixed address `192.168.4.20` and serves
its picture over HTTP; the fly host pulls frames from it exactly like it
would from a phone. Sketch: `FruitFlyPrototype/ESP32CAM/`.

### Power

```text
ESP32-CAM           supply
  5V   ───────────  5 V rail (same rail as the HC-SR04 / ESP32 VIN)
  GND  ───────────  common GND
```

It draws up to ~300 mA in bursts while streaming over Wi-Fi. Feed it from
the 5 V buck output, never from the main ESP32's 3.3 V pin. Brown-outs show
up as the board rebooting every few seconds while streaming.

### Mounting

Facing forward at bumper height, just above the ultrasonic, lens clear of
any chassis edge. If the picture is upside down or mirrored, set
`CAM_FLIP_V` / `CAM_MIRROR_H` in `ESP32CAM/config.h` rather than re-mounting.

### Endpoints (once it has joined the AP)

| URL | What |
|---|---|
| `http://192.168.4.20/stream` | MJPEG stream — what the fly host consumes |
| `http://192.168.4.20/capture` | one JPEG (open in a browser to check focus/orientation) |
| `http://192.168.4.20/` | status JSON: camera ok, frames served, RSSI, PSRAM, uptime |

Host usage: `./run.sh --camera http://192.168.4.20/stream ...` (or the same
flag on `fruitfly_bridge.py`). No phone camera is needed then.

### Flashing

The AI Thinker board has no USB. Use a USB-TTL adapter (3.3 V logic, 5 V
power) or the ESP32-CAM-MB base board.

```text
USB-TTL          ESP32-CAM
  5V   ────────  5V
  GND  ────────  GND
  TX   ────────  U0R
  RX   ────────  U0T
                 GPIO 0 ──── GND   (jumper, only while flashing)
```

1. Fit the GPIO 0 → GND jumper, press the board's RESET button (bootloader mode).
2. Compile and upload:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32cam --export-binaries FruitFlyPrototype/ESP32CAM
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32cam FruitFlyPrototype/ESP32CAM
```

3. Remove the jumper, press RESET again.
4. Serial (115200) should show `[CAMERA] ready`, then `[WIFI] up 192.168.4.20`
   once the robot's AP is on. Open `http://192.168.4.20/capture` from a
   device on the `RobotPrototype` network to see a frame.

Board in the IDE: **AI Thinker ESP32-CAM** (fqbn `esp32:esp32:esp32cam`).
Pre-built binaries are in `ESP32CAM/build/esp32.esp32.esp32cam/`.

### Config knobs (`ESP32CAM/config.h`)

| Setting | Default | Meaning |
|---|---|---|
| `CAM_FRAME_SIZE` | `FRAMESIZE_QVGA` | 320×240; the retina only uses 64×48, bigger just costs bandwidth |
| `CAM_JPEG_QUALITY` | 12 | 0 best … 63 worst; 10–14 is the sweet spot for MJPEG |
| `CAM_FLIP_V` / `CAM_MIRROR_H` | 0 / 0 | fix orientation in software |
| `STREAM_MIN_FRAME_MS` | 66 | caps the stream at ~15 fps |
| `CAM_IP` | 192.168.4.20 | fixed address on the robot's network |
| `ENABLE_ESPNOW_FALLBACK` | off | also send ESP-NOW heartbeats / serial test commands to the robot; needs `ENABLE_ESPNOW` in `RobotPrototype/config.h` |

## 7. Bring-up order

1. Flash with no sensors attached. Serial should show `[TOF] ... init
   FAILED`, `[CLIFF] ... Disabled` (or `WARNING ... running WITHOUT` once
   enabled), `[OLED] Nothing at 0x3C — display disabled` and
   `[BUMP] Disabled` (or `front=HIT rear=HIT` once enabled with floating
   inputs — expected until the switches and pull-ups are on).
2. Add the bump switches + pull-ups, uncomment `ENABLE_BUMP`, re-flash. Boot
   line must read `front=clear rear=clear`. Press each: `[BUMP] front
   pressed / released`.
3. Add the ultrasonic. `[STATUS] dist=` should track a hand.
4. Plug in the OLED. Boot: `[OLED] SSD1306 ready`; the screen shows `STOP`
   and the range.
5. Add the cliff ToFs one at a time, uncomment `ENABLE_TOF_CLIFF`. Boot
   line: `[TOF] cliff-front ready at 0x31`. Read the flat-floor value, set
   `CLIFF_FLOOR_MM`, re-flash.
6. Hold the robot over a table edge with the wheels lifted:
   `[CLIFF] *** front drop detected ... latched ***`, web UI shows the red
   banner, FORWARD is refused, BACKWARD accepted, Clear E-Stop releases.
7. Power the ESP32-CAM; `http://192.168.4.20/capture` shows the view ahead.

## 8. Upload

Main controller:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries FruitFlyPrototype/RobotPrototype
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 FruitFlyPrototype/RobotPrototype
```

Camera board (GPIO 0 jumper to GND while uploading, see §6):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32cam --export-binaries FruitFlyPrototype/ESP32CAM
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32cam FruitFlyPrototype/ESP32CAM
```

Libraries required for the main controller (`arduino-cli lib install ...`,
or Library Manager in the IDE): **VL53L0X by Pololu**, **Adafruit SSD1306**,
**Adafruit GFX Library** (pulls in Adafruit BusIO). The camera sketch needs
nothing beyond the ESP32 core. Pre-built binaries are in
`RobotPrototype/build/esp32.esp32.esp32/` and
`ESP32CAM/build/esp32.esp32.esp32cam/`.
