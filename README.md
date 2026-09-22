# ESP32 4-Wheel Robot Prototype

A modular, safety-first 4-wheel robot controller built for ESP32 and ESP32-CAM using pure Arduino C++ (Arduino IDE 2.x & Arduino CLI compatible).

> **Building the two-wheel robot?** This folder is the original four-motor
> reference design and its firmware drives four motors. The robot actually
> on the bench is the two-motor FruitFly build in
> [`FruitFlyPrototype/`](FruitFlyPrototype/README.md) — flash
> `FruitFlyPrototype/RobotPrototype`, and use
> [`FruitFlyPrototype/WIRING.md`](FruitFlyPrototype/WIRING.md) for the GPIO
> map. The pin tables below apply to the four-motor design only.

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
         DRV8833 #1–#4       Ultrasonic Sensor     ESP-NOW Receiver
         4 × BO Motors        (HC-SR04 + Divider)         ▲
                                                          │ ESP-NOW (Ch 1)
                                                   ┌──────┴──────┐
                                                   │  ESP32-CAM  │
                                                   │  Fallback   │
                                                   └─────────────┘
```

> **Safety Invariant**: The main ESP32 is the **only** device that directly manipulates motor GPIO. Remote sources (Phone, ESP32-CAM, Serial) can only produce commands that pass through arbitration and local obstacle safety checks.

---

## 2. Command Arbitration & Priority

Commands are arbitrated in the `ControlManager` with strict priority:

1. **Emergency STOP** (Immediate motor power cut)
2. **Local Obstacle Safety** (Ultrasonic hysteresis: stop at $\le 30\text{ cm}$, clear at $\ge 35\text{ cm}$)
3. **PHONE_WIFI** (Primary phone control over Access Point)
4. **PHONE_BLUETOOTH** (Optional fallback, compile-guarded)
5. **ESP32_CAM_ESPNOW** (Secondary controller fallback over ESP-NOW)
6. **NONE → STOP** (Failsafe stop if no valid heartbeat received within $1500\text{ ms}$)

### Failover Timeline

```text
Phone Wi-Fi Heartbeat (500 ms)
         │
         ├── Missing for > 1500 ms?
         ▼
Failover to Bluetooth (if enabled) or ESP32-CAM ESP-NOW
         │
         ├── Missing for > 1500 ms?
         ▼
Automatic Motor STOP (Failsafe)
```

---

## 3. Hardware & GPIO Wiring

### A. Motor Drivers (4 × DRV8833 Modules)

Each motor is assigned to its own dedicated DRV8833 H-bridge:

| Motor | DRV8833 Board | ESP32 IN1 | ESP32 IN2 | Inversion Config (`config.h`) |
|---|---|---|---|---|
| **Front Left** | Module #1 | **GPIO 16** | **GPIO 17** | `FL_INVERT` |
| **Front Right** | Module #2 | **GPIO 18** | **GPIO 19** | `FR_INVERT` |
| **Rear Left** | Module #3 | **GPIO 21** | **GPIO 22** | `RL_INVERT` |
| **Rear Right** | Module #4 | **GPIO 23** | **GPIO 25** | `RR_INVERT` |

* **Power**: Connect external motor power (regulated 5V–6V or dedicated battery) directly to `VM` on all DRV8833 boards.
* **Ground**: Connect battery GND, DRV8833 GND, and ESP32 GND together (**common ground**).
* **Warning**: **Never** power motors from the ESP32 3.3V or 5V regulator pin!

### B. Ultrasonic Sensor (HC-SR04)

* `VCC` → 5V
* `GND` → Common Ground
* `TRIG` → **GPIO 26**
* `ECHO` → **GPIO 27** via **Voltage Divider**:
  ```text
  HC-SR04 ECHO (5V) ──[ 1 kΩ ]──┬── GPIO 27 (3.3V safe)
                                │
                             [ 2 kΩ ]
                                │
                               GND
  ```

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
│   └── web_server.h / .cpp       # WebServer + embedded HTML5 Mobile UI
│
├── ESP32CAMFallback/             # ESP32-CAM fallback sketch
│   ├── ESP32CAMFallback.ino      # Heartbeat beacon & serial command bridge
│   ├── config.h                  # Channel & main ESP32 MAC configuration
│   ├── espnow_control.h / .cpp   # ESP-NOW transmitter logic
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

No third-party libraries or PlatformIO installations are required.

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
   * Open `ESP32CAMFallback/ESP32CAMFallback.ino`
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
arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAMFallback

# Upload to ESP32-CAM (replace COM4 with your port)
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32cam ESP32CAMFallback
```

---

## 6. Pairing & Configuration

1. **Upload `RobotPrototype`** to the main ESP32.
2. Open Serial Monitor at **115200 baud**. Note the MAC address output:
   ```text
   [BOOT] MAC Address: 24:0A:C4:XX:XX:XX
   ```
3. Open `ESP32CAMFallback/config.h` and update `MAIN_ESP32_MAC` with this address.
4. Upload `ESP32CAMFallback` to the ESP32-CAM. Note its MAC address from the Serial Monitor.
5. In `RobotPrototype/config.h`, update `ESP32_CAM_MAC` with the CAM's address for unicast communication (defaults to broadcast `0xFF` until configured).

---

## 7. Verification Checklist & Testing Phases

### A. Software & Build Verification (Completed)
- [x] Arduino IDE project structure compatibility (pure standard `.ino`, `.h`, `.cpp`)
- [x] Arduino CLI compilation — Main Controller (`RobotPrototype`)
- [x] Arduino CLI compilation — Fallback Controller (`ESP32CAMFallback`)
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
- [ ] Four motors (direction inversion calibration)
- [ ] Forward movement
- [ ] Backward movement
- [ ] Left differential turn
- [ ] Right differential turn
- [ ] In-place rotation (left & right)
- [ ] Normal Stop
- [ ] Latched Emergency Stop & Clear
- [ ] PWM speed scaling (0–255)
- [ ] Ultrasonic distance accuracy with voltage divider
- [ ] Obstacle safety hysteresis (stop at ≤30 cm, clear at ≥35 cm)
- [ ] Wi-Fi Soft AP connection (`RobotPrototype`)
- [ ] Phone web UI control
- [ ] Wi-Fi heartbeat timeout failover (1500 ms)
- [ ] ESP-NOW peer heartbeat receipt (500 ms)
- [ ] ESP-NOW remote movement commands
- [ ] ESP-NOW failover when Wi-Fi disconnects
- [ ] Failsafe motor halt when all controllers disconnect

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
