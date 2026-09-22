# ESP32 2-Wheel Robot Prototype (FruitFly fork)

A modular, safety-first **two-motor** robot controller built for ESP32 using pure Arduino C++ (Arduino IDE 2.x & Arduino CLI compatible). The original four-motor project is unchanged in `C:\Projects\RC\RobotPrototype`.

> **FruitFly fork.** This copy is driven by a host-side fruit-fly connectome (`FruitFlyHost/`) and uses two motors. What differs from the base firmware described below:
>
> * **Host in charge, firmware has the veto.** The host (laptop, or the phone via Termux + Debian — see `PHONE_GUIDE.md` for the copy-paste setup a friend can follow, `SETUP_TERMUX.md` for the short version) runs the brain and posts intents to `/api/command`; the ESP32 still enforces range stop, bumpers, cliffs, e-stop and heartbeat timeout.
> * **Two ways to drive.** The host dashboard (`http://<host>:8642/`) shows the brain; `http://<host>:8642/controls` is a phone page with a *Fly brain / Manual* switch and a hold-to-drive pad. Manual mode works without `--enable-motors`.
> * **Speeds.** Host defaults are now `--max-speed 200` and `--min-speed 120` (PWM out of 255); the brushed BO motors stall below ~110, so the host never sends less than the floor.
> * **Avoidance is a saccade state machine** (`FruitFlyHost/fly_controller.py`): trigger (DNp01 looming, range sensor or front bumper) → freeze → back up (range/bump triggers) → rotate **until the way is clear** (range ≥ 45 cm and no loom, with min/max duration) → refractory. Escapes close together keep the same turning direction so the robot does not ping-pong; the third one inside 8 s backs up longer and turns roughly twice as far.
> * **`SWAP_LEFT_RIGHT = true`** in `RobotPrototype/config.h`: the chassis drivers were wired mirrored, so LEFT turned right; the firmware swaps the pin tables. Set false if re-wired.
> * **Sensors:** HC-SR04 front range on 26/27 (LD2420 and VL53L0X selectable), endstop bumpers on 34/35 (`ENABLE_BUMP`), downward VL53L0X cliff pair (`ENABLE_TOF_CLIFF`), and an **SSD1306 OLED** status display on the I²C bus (`ENABLE_OLED`, on by default, auto-disables when absent). All wiring in `WIRING.md`.
> * **Camera.** The phone is the default camera (browser push page or IP Webcam). The `ESP32CAM/` sketch (replaces `ESP32CAMFallback/`) turns an AI Thinker ESP32-CAM into a Wi-Fi camera on the robot's AP at `192.168.4.20/stream`; use `--camera http://192.168.4.20/stream`. Its ESP-NOW fallback role is optional (`ENABLE_ESPNOW_FALLBACK` + `ENABLE_ESPNOW`), off by default.
>
> See `FRUITFLY_SETUP.md`, `WIRING.md`, `FruitFlyHost/README.md`, `SETUP_TERMUX.md` and `PHONE_GUIDE.md`. The sections below describe the shared base firmware.
---

## 1. System Architecture

```text
                         PHONE (Wi-Fi AP / Web UI)
                                    │
                                    ▼
                         ┌────────────────────┐
                         │    ESP32 DEVKIT    │
                         │  (Main Controller) │
                         │                    │
                         │ • Command Parser   │
                         │ • Control Manager  │
                         │ • Safety Manager   │
                         │ • Motor Controller │
                         └─────────┬──────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              ▼                    ▼                    ▼
        DRV8833 #1 + #2      Front range sensor    Bump / cliff / OLED
        2 × BO Motors        (HC-SR04 default)     (compile-guarded)
        (bridges paralleled)
```

An ESP32-CAM, when fitted, is a plain Wi-Fi camera on the same AP
(`192.168.4.20/stream`). Its ESP-NOW fallback role is compiled out by
default (`ENABLE_ESPNOW`).

> **Safety Invariant**: The main ESP32 is the **only** device that directly manipulates motor GPIO. Remote sources (Phone, host bridge, Serial) can only produce commands that pass through arbitration and local obstacle safety checks.

---

## 2. Command Arbitration & Priority

Commands are arbitrated in the `ControlManager` with strict priority:

1. **Emergency STOP** (Immediate motor power cut)
2. **Local Obstacle Safety** (Ultrasonic hysteresis: stop at $\le 30\text{ cm}$, clear at $\ge 35\text{ cm}$)
3. **PHONE_WIFI** (Primary phone control over Access Point)
4. **PHONE_BLUETOOTH** (Optional fallback, compile-guarded)
5. **ESP32_CAM_ESPNOW** (Secondary controller fallback over ESP-NOW — compiled out by default in this fork)
6. **NONE → STOP** (Failsafe stop if no valid heartbeat received within $1500\text{ ms}$)

### Failover Timeline

```text
Phone Wi-Fi Heartbeat (500 ms)
         │
         ├── Missing for > 1500 ms?
         ▼
Failover to Bluetooth / ESP-NOW (both disabled by default here)
         │
         ├── Missing for > 1500 ms?
         ▼
Automatic Motor STOP (Failsafe)
```

---

## 3. Hardware & GPIO Wiring

### A. Motor Drivers (2 × DRV8833 Modules)

#### Two-motor setup: bridged for higher current

If each motor's startup or stall current can approach 1.5 A, use **two
DRV8833 modules**, one module per motor, with both internal H-bridges
paralleled on each module. This gives the theoretical combined capability of
the two bridges on that module, subject to the exact IC package, PCB traces,
cooling, supply, and motor current profile.

```text
Driver module 1 → Motor 1:
  ESP32 GPIO 16 ──┬── IN1
                  └── IN3
  ESP32 GPIO 17 ──┬── IN2
                  └── IN4
  OUT1 + OUT3 ───── motor 1 terminal 1
  OUT2 + OUT4 ───── motor 1 terminal 2

Driver module 2 → Motor 2:
  ESP32 GPIO 18 ──┬── IN1
                  └── IN3
  ESP32 GPIO 19 ──┬── IN2
                  └── IN4
  OUT1 + OUT3 ───── motor 2 terminal 1
  OUT2 + OUT4 ───── motor 2 terminal 2
```

For both modules: `VCC` goes to the regulated 5–6 V motor rail, `GND` goes to
common ground, and `EEP` is driven HIGH by the firmware from **GPIO23**. `ULT`
may remain disconnected until fault monitoring is added.

If each motor is comfortably below one bridge's continuous and stall-current
limits, a single module can instead drive both motors normally using `IN1/IN2`
and `IN3/IN4` — but then GPIO 18/19 feed that same module's second input pair.

Each motor is assigned to its own DRV8833 module. For higher current, this
project uses the two H-bridges **inside each DRV8833 in parallel**. This is
supported by the TI DRV8833 datasheet, but the exact current limit depends on
the IC package and thermal performance. The common PWP/RTY package is rated
up to 3 A RMS / 4 A peak in parallel at the stated conditions; small carrier
boards and the PW package may be lower. Always check the actual module and
motor stall current.

Do not connect the outputs of two separate DRV8833 chips together. Only
parallel the A and B bridges on the same chip, as described below.

#### Motor and control mapping

| Motor | DRV8833 Board | ESP32 IN1 | ESP32 IN2 | Inversion Config (`config.h`) |
|---|---|---|---|---|
| **Left** | Module #1 | **GPIO 16** | **GPIO 17** | `FL_INVERT` |
| **Right** | Module #2 | **GPIO 18** | **GPIO 19** | `FR_INVERT` |
| *(both modules)* | `EEP` enable | **GPIO 23** | — | driven HIGH at boot |

Only these two motors exist in this build. GPIO 21/22 are the I²C bus
(VL53L0X + OLED) and GPIO 25 is the forward VL53L0X `XSHUT` — they are **not**
rear-motor pins any more. The `RL_*` / `RR_*` names still in `config.h` are
inherited from the four-motor original and are never initialised or driven.

**Left and right swapped?** `SWAP_LEFT_RIGHT = true` in `config.h` exchanges
the two pin sets in software, because this chassis was wired with the modules
mirrored. Set it `false` only after re-wiring the harness. Forward/backward
are unaffected either way.

For each module in bridged mode, use this board's actual labels:

```text
ESP32 IN1 ─────┬── IN1
               └── IN3

ESP32 IN2 ─────┬── IN2
               └── IN4

OUT1 ──────────┬── motor terminal 1
OUT3 ──────────┘

OUT2 ──────────┬── motor terminal 2
OUT4 ──────────┘
```

#### Complete per-module connection table

| Driver connection | Module #1 (left) | Module #2 (right) |
|---|---|---|
| `IN1` + `IN3` | GPIO 16 | GPIO 18 |
| `IN2` + `IN4` | GPIO 17 | GPIO 19 |
| `OUT1` + `OUT3` | left motor lead 1 | right motor lead 1 |
| `OUT2` + `OUT4` | left motor lead 2 | right motor lead 2 |
| `VCC` | regulated motor rail | regulated motor rail |
| `GND` | common ground | common ground |
| `EEP` (sleep/enable) | GPIO 23 HIGH | GPIO 23 HIGH |
| `ULT` (fault) | leave open or monitor | leave open or monitor |

On this board family, `EEP` is the sleep/enable input. Keep the factory
enable jumper installed or connect `EEP` to a logic-high supply as documented
by the board. Do not leave the sleep input floating. `ULT` is the fault output
and can later be connected to an unused ESP32 input for driver fault reporting.
The exact physical pin names
vary between carrier boards, so use the board schematic rather than assuming
the bare-IC pin numbers.

Use short, equal-length connections between the paired outputs. Tie the
inputs before they enter the driver board where practical. Keep the motor
current wiring separate from the ESP32 signal wiring.

#### Power architecture

```text
Battery pack
   │
   ├── fuse + master switch ── buck converter 5–6 V ── VCC on all DRV8833 boards
   │                                                └── motor supply rail
   │
   └── buck converter 5 V ── ESP32 5V/VIN

Battery GND ──┬── driver GNDs
              ├── ESP32 GND
              └── sensor GND
```

With two motors the peak draw is roughly half the original four-motor
figure, but still size the pack, fuse and buck converter for **stall**
current, not running current.

Recommended baseline for 5–6 V motors is a 2S Li-ion/LiPo pack (7.4 V
nominal, 8.4 V full) followed by a regulated 5–6 V motor buck converter.
The DRV8833 motor supply must remain within 2.7–10.8 V, and a 6 V motor must
not be run continuously above its rated voltage. Size the battery, fuse, and
buck converter for the combined motor stall/start current, not just the
no-load running current. Use a proper charger and BMS for the battery
chemistry; never charge a Li-ion/LiPo pack from the ESP32 or motor supply.

Place the recommended ceramic and bulk capacitors close to each driver
module. As a practical starting point, use the module's required local VCC
capacitor plus roughly 470–1000 µF electrolytic across the motor rail near
the driver group. Use a fuse and preferably a separate power switch for the
motor rail. Do not power the motors from the ESP32 3.3 V or 5 V regulator.

* **Power**: Connect external motor power (regulated 5V–6V or dedicated battery) directly to `VCC` on all DRV8833 boards.
* **Ground**: Connect battery GND, DRV8833 GND, and ESP32 GND together (**common ground**).
* **Warning**: **Never** power motors from the ESP32 3.3V or 5V regulator pin!

#### Important bridge-mode checks

* Confirm the carrier board exposes `IN1`–`IN4` and `OUT1`–`OUT4`.
* In the current two-motor firmware, `EEP` is the sleep/enable input and is
  held HIGH from GPIO23. LOW puts the driver to sleep.
* `ULT` is the fault indication output and can remain unconnected for now;
  connect it to an ESP32 input with a pull-up later if fault telemetry is
  desired.
* Do not exceed the motor's stall current or the driver/module thermal limit.
  Current limiting and protection do not make an undersized power supply safe.
* Test one motor at low PWM first, then check each bridged module for heat
  before running both motors under load.

### B. Front range sensor

`config.h` selects exactly one of three front sensors. The default is
`#define FRONT_RANGE_HCSR04`; the alternatives are `FRONT_RANGE_LD2420`
(presence radar) and `FRONT_RANGE_VL53L0X` (ToF on I²C, `XSHUT` on GPIO 25).

#### HLK-LD2420 presence radar (`FRONT_RANGE_LD2420`)

```text
LD2420 3V3 → ESP32 3V3
LD2420 GND → ESP32 GND
LD2420 OT2 → ESP32 GPIO27
```

On some LD2420 firmware revisions the presence output is labelled `OT1`
instead of `OT2`; verify the module's manual. HIGH means presence and LOW
means clear. The current firmware treats presence as an obstacle and reports
`distance_cm: 0.0`. UART configuration and actual range data are not yet used.

#### HC-SR04 ultrasonic (`FRONT_RANGE_HCSR04`, default)

* `VCC` → 5V
* `GND` → Common Ground
* `TRIG` → **GPIO 26**
* `ECHO` → **GPIO 27** via a **1 kΩ / 2 kΩ voltage divider**:
  ```text
  HC-SR04 ECHO (5V) ──[ 1 kΩ ]──┬── GPIO 27 (3.3 V safe)
                                │
                             [ 2 kΩ ]
                                │
                               GND
  ```

A 10 kΩ / 20 kΩ divider works electrically too; `WIRING.md` specifies 1 k/2 k
because the lower impedance picks up less motor noise on a long ECHO lead.
The echo edge is read by interrupt, so the main loop never blocks.

Bump switches, cliff sensors, the OLED and the complete GPIO map are in
`WIRING.md`.

The ESP32, driver boards, HC-SR04, and buck converter output must share a
common ground. Keep the HC-SR04 ECHO divider physically close to GPIO 27 and
never connect the 5 V ECHO signal directly to the ESP32.

### C. ESP32 power and logic connections

* Regulated 5 V buck output → ESP32 `5V`/`VIN` pin, according to the exact
  DevKit board labeling.
* ESP32 `GND` → the common ground bus.
* ESP32 GPIO outputs → driver input pairs listed above.
* Do not connect the motor battery or the 5–6 V motor rail to the ESP32
  `3V3` pin.
* Do not connect a 5 V logic signal to an ESP32 GPIO. The HC-SR04 ECHO line
  specifically requires the divider shown above.

Use a buck converter with enough current capacity for the ESP32's Wi-Fi
bursts and any attached peripherals. A separate motor buck converter keeps
motor noise and voltage dips away from the ESP32 supply.

### D. Battery and grounding checklist

1. Battery positive → fuse → master switch → split to the motor buck and the
   ESP32 buck.
2. Battery negative → common ground bus.
3. Motor buck output positive → every driver `VCC` pin.
4. Motor buck output negative → every driver `GND` pin and ESP32 `GND`.
5. ESP32 buck output positive → ESP32 `5V`/`VIN`.
6. HC-SR04 `VCC` → regulated 5 V and `GND` → common ground.
7. Verify polarity and voltage with a multimeter before inserting the ESP32
   or motors.

The common ground is required for the ESP32 GPIO logic levels to have a
shared reference. Route high-current battery and motor traces separately from
the thin GPIO and sensor wires, joining them at the power-ground star point.

---

## 4. Software Structure

```text
c:/Projects/RC/
├── RobotPrototype/               # Main ESP32 sketch
│   ├── RobotPrototype.ino        # setup(), non-blocking loop()
│   ├── config.h                  # GPIO mapping, constants, thresholds
│   ├── command.h                 # RobotCommand & RobotPacket structures
│   ├── robot_state.h / .cpp      # Central telemetry & state
│   ├── motor_controller.h / .cpp # DRV8833 PWM (analogWrite) & kinematics
│   ├── ultrasonic.h / .cpp       # Non-blocking HC-SR04 pulse measurement
│   ├── safety_manager.h / .cpp   # Obstacle hysteresis & E-stop enforcement
│   ├── command_parser.h / .cpp   # Serial / JSON / ESP-NOW packet parser
│   ├── control_manager.h / .cpp  # Priority arbitration & timeout failover
│   ├── wifi_control.h / .cpp     # Wi-Fi Soft AP (SSID: RobotPrototype, Ch: 1)
│   ├── espnow_control.h / .cpp   # ESP-NOW receiver with peer validation
│   ├── bluetooth_control.h / .cpp# Optional Bluetooth SPP fallback
│   ├── web_server.h / .cpp       # WebServer + embedded HTML5 Mobile UI
│   ├── bump.h / .cpp             # Endstop bumpers (ENABLE_BUMP)
│   ├── tof.h / .cpp, cliff.h / .cpp # VL53L0X bring-up and latched cliff detection
│   └── oled.h / .cpp             # SSD1306 status display (ENABLE_OLED)
│
├── ESP32CAM/                     # ESP32-CAM as a Wi-Fi camera (FruitFly fork)
│   ├── ESP32CAM.ino              # joins the robot AP, serves /stream /capture /
│   ├── config.h                  # Wi-Fi, fixed IP, frame size, optional ESP-NOW fallback
│   ├── espnow_control.h / .cpp   # ESP-NOW transmitter (ENABLE_ESPNOW_FALLBACK only)
│
├── INSTRUCTION.md                # Base requirements specification
└── README.md                     # This documentation
```

---

## 5. Setup & Compilation

### Required Libraries
All libraries are standard and included with the official **ESP32 Arduino Core by Espressif** (version 2.x or 3.x):
* `WiFi`
* `WebServer`
* `esp_now`
* `BluetoothSerial` (used only if `#define ENABLE_BLUETOOTH` is uncommented)
* `esp_camera` (included with ESP32 board package for ESP32-CAM)

FruitFly fork additions (main controller only): **VL53L0X by Pololu**,
**Adafruit SSD1306**, **Adafruit GFX Library** —
`arduino-cli lib install VL53L0X "Adafruit SSD1306" "Adafruit GFX Library"`.
No PlatformIO installation is required.

### A. Arduino IDE 2.x
1. Open **Preferences** (`Ctrl + ,`) and add the ESP32 board manager URL:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. Open **Boards Manager**, search for `esp32`, and install **esp32 by Espressif Systems**.
3. For Main Controller:
   * Open `RobotPrototype/RobotPrototype.ino`
   * Select your board (e.g. `ESP32 Dev Module`) and target COM port.
   * Click **Upload**.
4. For ESP32-CAM:
   * Open `ESP32CAM/ESP32CAM.ino`
   * Select your board (e.g. `AI Thinker ESP32-CAM` or your physical module) and COM port.
   * Click **Upload**.

### B. Arduino CLI
```bash
# Update indices and install core
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Compile Main ESP32
arduino-cli compile --fqbn esp32:esp32:esp32 RobotPrototype

# Upload to Main ESP32 (replace COM3 with your port)
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 RobotPrototype

# Compile ESP32-CAM
arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM

# Upload to ESP32-CAM (replace COM4 with your port; GPIO 0 to GND while flashing)
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32cam ESP32CAM
```

---

## 6. Pairing & Configuration

1. **Upload `RobotPrototype`** to the main ESP32.
2. Open Serial Monitor at **115200 baud**. Note the MAC address output:
   ```text
   [BOOT] MAC Address: 24:0A:C4:XX:XX:XX
   ```
3. Open `ESP32CAM/config.h` and update `MAIN_ESP32_MAC` with this address.
4. Upload `ESP32CAM` to the ESP32-CAM. Note its MAC address from the Serial Monitor.
5. In `RobotPrototype/config.h`, update `ESP32_CAM_MAC` with the CAM's address for unicast communication (defaults to broadcast `0xFF` until configured).

In the FruitFly fork this pairing is optional: ESP-NOW is compiled out on
both sides (`ENABLE_ESPNOW` / `ENABLE_ESPNOW_FALLBACK`) and the ESP32-CAM
works purely as a Wi-Fi camera without any of the steps above.

---

## 7. Verification Checklist & Testing Phases

### A. Software & Build Verification (Completed)
- [x] Arduino IDE project structure compatibility (pure standard `.ino`, `.h`, `.cpp`)
- [x] Arduino CLI compilation — Main Controller (`RobotPrototype`)
- [x] Arduino CLI compilation — ESP32-CAM camera sketch (`ESP32CAM`)
- [x] Decoupled ESP-NOW receive callback (lightweight ISR copy to queue)
- [x] Unconditional normal STOP acceptance from any source
- [x] Latched Emergency Stop with explicit Clear Emergency command
- [x] Sender MAC filtering with unconfigured development-mode warning
- [x] ESP-NOW sequence validation with wraparound handling
- [x] Independent heartbeat processing
- [x] Ultrasonic bounded timeout (non-blocking loop)
- [x] No PlatformIO dependency

### B. Physical Hardware Verification (To be completed on bench)
- [ ] One motor (direction & speed test)
- [ ] Both motors (direction inversion + `SWAP_LEFT_RIGHT` calibration)
- [ ] Forward movement
- [ ] Backward movement
- [ ] Left differential turn
- [ ] Right differential turn
- [ ] In-place rotation (left & right)
- [ ] Normal Stop
- [ ] Latched Emergency Stop & Clear
- [ ] PWM speed scaling (0–255)
- [ ] Bridged DRV8833 modules stay cool under load
- [ ] Ultrasonic distance accuracy with voltage divider
- [ ] Obstacle safety hysteresis (stop at ≤30 cm, clear at ≥35 cm)
- [ ] Wi-Fi Soft AP connection (`RobotPrototype`)
- [ ] Phone web UI control
- [ ] Wi-Fi heartbeat timeout failover (1500 ms)
- [ ] Failsafe motor halt when all controllers disconnect
- [ ] Bump endstops halt the robot (`ENABLE_BUMP`)
- [ ] Cliff pair halts the robot at a table edge (`ENABLE_TOF_CLIFF`)
- [ ] OLED shows state, range and command source (`ENABLE_OLED`)

The ESP-NOW items from the base project do not apply here: the receiver is
compiled out in this fork.

---

## 8. Future AI Integration (Groq LLM Provider)

As specified in the future expansion roadmap (§33, §34):
* **Provider**: **Groq** will be utilized as the ultra-low-latency LLM inference provider for high-speed voice and vision reasoning on the host/phone layer.
* **Architecture**:
  * Edge host / phone runs vision/audio models and queries Groq API (e.g., `llama-3-70b` / `whisper-large-v3`).
  * Groq emits structured high-level JSON action intents:
    ```json
    { "action": "FORWARD", "speed": 150 }
    ```
  * **Crucial Rule**: The AI never has direct hardware pin access. All Groq-generated commands must pass through `CommandParser` $\to$ `ControlManager` $\to$ `SafetyManager` $\to$ `MotorController`. Local obstacle safety cannot be overridden by AI hallucination or communication latency.
