# Wiring — FruitFly 2-motor build

Everything below matches `RobotPrototype/config.h`. Common ground for
battery, drivers, sensors and ESP32 is assumed throughout.

## GPIO map

| GPIO | Function | Notes |
|---|---|---|
| 16, 17 | Motor FL IN1/IN2 (DRV8833 #1, bridges paralleled) | |
| 18, 19 | Motor FR IN1/IN2 (DRV8833 #2, bridges paralleled) | |
| 23 | DRV8833 `EEP` enable (both modules) | driven HIGH at boot |
| 26 | HC-SR04 `TRIG` | only with `FRONT_RANGE_HCSR04` |
| 27 | HC-SR04 `ECHO` (via divider) **or** LD2420 `OT2` | one or the other |
| 21 | I²C SDA | all VL53L0X |
| 22 | I²C SCL | all VL53L0X |
| 25 | VL53L0X `XSHUT`, forward-facing | only with `FRONT_RANGE_VL53L0X` |
| 32 | VL53L0X `XSHUT`, cliff front | |
| 33 | VL53L0X `XSHUT`, cliff rear | |
| 34 | Front bump endstop | input-only, needs external pull-up |
| 35 | Rear bump endstop | input-only, needs external pull-up |

Free for later: 0 (boot strap, avoid), 2 (LED), 4, 5, 12–15 (JTAG/strapping, use with care), 36/39 (input-only ADC).

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

## 2. Limit-switch bumpers (3D-printer endstops)

Use the **mechanical microswitch** endstops (the ones with a metal lever),
not the optical ones. Each switch has three tabs: `C` (common), `NC`
(normally closed), `NO` (normally open). Some boards label them `S / - / +`
or have a 3-pin JST — on those, use the bare switch tabs directly, or
check which two pins are closed at rest with a multimeter.

Wire **normally closed**, so a broken wire or unplugged connector reads as
"pressed" and the robot refuses to drive into it:

```text
                     3.3 V
                       │
                    [10 kΩ]
                       │
GPIO 34 (front) ───────┼──────── C  (common)
                       │           \
                       │            \  switch, lever unpressed = closed
                       │             \
                      GND ───────── NC
```

Same for the rear switch on **GPIO 35**. Resting level LOW (closed to
ground), pressed = HIGH.

The 10 kΩ pull-up is **required**: GPIO 34/35 are input-only pins on the
ESP32 with no internal pull-ups. Put the resistor close to the ESP32.

Two switches per bumper (left and right end of a bar) can simply be wired
in **series** on the same GPIO — any one opening trips it.

Mechanics: a bumper bar (3D-printed or a strip of plastic) hinged so that a
push anywhere along it presses the lever. Mount it 5–10 mm proud of the
chassis so it trips *before* the chassis hits.

Behaviour: front pressed → FORWARD, LEFT, RIGHT and both rotations are
blocked, BACKWARD allowed. Rear pressed → mirror. Clears automatically when
the switch releases (not latched).

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

## 5. Bring-up order

1. Flash with no sensors attached. Serial should show `[TOF] ... init
   FAILED`, `[CLIFF] WARNING ... running WITHOUT`, and `[BUMP] ... front=HIT
   rear=HIT` (floating inputs read HIGH — expected until the switches and
   pull-ups are on).
2. Add the bump switches + pull-ups. Boot line must read `front=clear
   rear=clear`. Press each: `[BUMP] front pressed / released`.
3. Add the ultrasonic. `[STATUS] dist=` should track a hand.
4. Add the cliff ToFs one at a time. Boot line: `[TOF] cliff-front ready at
   0x31`. Read the flat-floor value, set `CLIFF_FLOOR_MM`, re-flash.
5. Hold the robot over a table edge with the wheels lifted:
   `[CLIFF] *** front drop detected ... latched ***`, web UI shows the red
   banner, FORWARD is refused, BACKWARD accepted, Clear E-Stop releases.

## 6. Upload

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries FruitFlyPrototype/RobotPrototype
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 FruitFlyPrototype/RobotPrototype
```

Requires the **VL53L0X by Pololu** library (`arduino-cli lib install
VL53L0X`, or Library Manager in the IDE). Pre-built binaries are in
`RobotPrototype/build/esp32.esp32.esp32/`.
