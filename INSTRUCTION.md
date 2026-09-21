# Robot Prototype Instructions
## Arduino IDE / Arduino CLI

## 1. Objective

Build the first working prototype of a 4-wheel ESP32 robot using:

- ESP32 DevKit as the main controller
- ESP32-CAM as an ESP-NOW fallback controller
- 4 × brushed DC BO motors
- 4 × DRV8833 motor-driver boards
- Ultrasonic obstacle detection
- Phone control over Wi-Fi
- Optional Bluetooth fallback
- Local safety and command arbitration on the main ESP32

This project must be written as **Arduino C++ compatible with both Arduino IDE and Arduino CLI**.

Do **not** use PlatformIO.

Do not implement AI, computer vision, gesture recognition, Telegram, cloud services, or person following in this first prototype.

The goal is to create a reliable hardware and communication foundation that can later support those features.

---

# 2. Arduino Environment

Use:

- Arduino IDE 2.x and/or Arduino CLI
- ESP32 Arduino Core
- Standard Arduino project structure
- `.ino`, `.h`, and `.cpp` files

Do not require:

- PlatformIO
- `platformio.ini`
- PlatformIO-specific libraries
- PlatformIO-specific build scripts
- CMake
- ESP-IDF-only APIs unless they are exposed and supported by the installed ESP32 Arduino core

The project should compile using Arduino IDE and Arduino CLI.

---

# 3. Arduino Project Structure

Use a normal Arduino sketch folder.

Main ESP32:

```text
RobotPrototype/
├── RobotPrototype.ino
├── config.h
│
├── motor_controller.h
├── motor_controller.cpp
│
├── ultrasonic.h
├── ultrasonic.cpp
│
├── command.h
├── command_parser.h
├── command_parser.cpp
│
├── control_manager.h
├── control_manager.cpp
│
├── safety_manager.h
├── safety_manager.cpp
│
├── wifi_control.h
├── wifi_control.cpp
│
├── bluetooth_control.h
├── bluetooth_control.cpp
│
├── espnow_control.h
├── espnow_control.cpp
│
├── web_server.h
├── web_server.cpp
│
├── robot_state.h
└── robot_state.cpp
```

ESP32-CAM:

```text
ESP32CAMFallback/
├── ESP32CAMFallback.ino
├── config.h
├── espnow_control.h
└── espnow_control.cpp
```

The folders must be directly openable as Arduino sketches.

The `.ino` file is the entry point and must contain:

```cpp
void setup();
void loop();
```

---

# 4. Required Arduino Libraries

Use libraries available through the Arduino Library Manager or built into the ESP32 Arduino core.

Prefer ESP32-core APIs where appropriate.

Expected dependencies may include:

- WiFi
- WebServer
- ESP-NOW support provided by the ESP32 Arduino core
- Bluetooth/BLE only if implemented
- ESP32 camera support for the ESP32-CAM

Do not introduce unnecessary third-party dependencies.

Document every required library in `README.md`.

---

# 5. System Architecture

```text
                         PHONE
                    ┌─────────────┐
                    │             │
                    │ Wi-Fi       │
                    │ Bluetooth   │
                    └──────┬──────┘
                           │
                           ▼
                  ┌─────────────────┐
                  │      ESP32      │
                  │ MAIN CONTROLLER │
                  │                 │
                  │ Command Manager │
                  │ Safety Manager  │
                  │ Motor Controller│
                  │ Sensor Manager  │
                  └────────┬────────┘
                           │
             ┌─────────────┼─────────────┐
             │             │             │
             ▼             ▼             ▼
          DRV8833       DRV8833       Sensors
             │             │
             ▼             ▼
          Motors         Motors

                     ESP-NOW
                         ▲
                         │
                  ┌──────┴──────┐
                  │ ESP32-CAM   │
                  │             │
                  │ Heartbeat   │
                  │ Test Cmds   │
                  │ Camera      │
                  └─────────────┘
```

The main ESP32 is the **only device allowed to directly control the motors**.

Phone and ESP32-CAM only send commands.

---

# 6. Control Priority

Use this priority:

```text
1. Emergency STOP
2. Local obstacle safety
3. PHONE_WIFI
4. PHONE_BLUETOOTH
5. ESP32_CAM_ESPNOW
6. NONE → STOP
```

Every command must pass through the main ESP32 safety system.

No remote source may bypass safety checks.

---

# 7. Communication Failover

Use application-level heartbeats.

Recommended heartbeat:

```text
500 ms
```

Recommended timeout:

```text
1500 ms
```

If a source stops sending valid heartbeats for longer than the timeout, mark it unavailable.

Do not rely only on TCP/Bluetooth connection state.

Expected behavior:

```text
Phone Wi-Fi available
        ↓
PHONE_WIFI controls robot

Phone Wi-Fi unavailable
        ↓
Phone Bluetooth available?
        ↓
PHONE_BLUETOOTH controls robot

Both unavailable
        ↓
ESP32-CAM heartbeat available?
        ↓
ESP32_CAM_ESPNOW controls robot

Nothing available
        ↓
STOP
```

---

# 8. Hardware

## Main controller

- ESP32 DevKit USB-C
- ESP32 expansion board

## Motor system

- 4 × brushed DC BO motors
- 4 × DRV8833 boards
- 4 × wheels
- 4-wheel chassis

Initial motor mapping:

| Driver | Motor |
|---|---|
| DRV8833 #1 | Front Left |
| DRV8833 #2 | Front Right |
| DRV8833 #3 | Rear Left |
| DRV8833 #4 | Rear Right |

Use one motor per DRV8833 board initially.

Do not parallel the two H-bridges during the first prototype.

---

# 9. ESP32 GPIO Mapping

Initial mapping:

| Motor | IN1 | IN2 |
|---|---:|---:|
| Front Left | GPIO16 | GPIO17 |
| Front Right | GPIO18 | GPIO19 |
| Rear Left | GPIO21 | GPIO22 |
| Rear Right | GPIO23 | GPIO25 |

Ultrasonic:

```text
TRIG = GPIO26
ECHO = GPIO27
```

Put all GPIO assignments in `config.h`.

Do not scatter GPIO numbers throughout the code.

Before wiring permanently, verify that the selected ESP32 board exposes these pins and that no board-specific function conflicts with them.

---

# 10. DRV8833 Wiring

For each driver:

```text
ESP32 GPIO
    │
    ├── IN1
    └── IN2

DRV8833
    │
    ├── OUT1 ─── Motor
    └── OUT2 ─── Motor
```

Motor supply:

```text
Motor battery / regulated motor supply
                │
                ▼
              VM
            DRV8833
```

Ground:

```text
Battery GND
    │
    ├── DRV8833 GND
    └── ESP32 GND
```

All grounds must be common.

Never power the motors from the ESP32 3.3 V output.

The exact DRV8833 breakout-board pinout must be verified from the actual board before wiring.

---

# 11. Ultrasonic Sensor

For a typical HC-SR04:

```text
VCC  → 5 V
GND  → GND
TRIG → GPIO26
ECHO → GPIO27 through voltage divider
```

A typical HC-SR04 ECHO output can be approximately 5 V.

ESP32 GPIO is not a 5 V input.

Example divider:

```text
HC-SR04 ECHO
      │
     1kΩ
      │
      ├──────── GPIO27
      │
     2kΩ
      │
     GND
```

Verify the actual sensor/module before connecting it.

---

# 12. Motor Commands

Define an enum for:

```text
FORWARD
BACKWARD
LEFT
RIGHT
ROTATE_LEFT
ROTATE_RIGHT
STOP
```

Speed:

```text
0–255
```

Create a common command structure.

Example:

```cpp
struct RobotCommand {
    uint8_t source;
    uint8_t action;
    uint8_t speed;
    uint32_t sequence;
    uint32_t timestamp;
};
```

Communication handlers must produce `RobotCommand`.

They must not directly manipulate motor GPIO.

Flow:

```text
Communication
      ↓
Command Parser
      ↓
Control Manager
      ↓
Safety Manager
      ↓
Motor Controller
```

---

# 13. Motor Behavior

Forward:

```text
FL = +speed
FR = +speed
RL = +speed
RR = +speed
```

Backward:

```text
FL = -speed
FR = -speed
RL = -speed
RR = -speed
```

Left:

```text
Left side  = reduced speed
Right side = increased speed
```

Right:

```text
Left side  = increased speed
Right side = reduced speed
```

Rotate left:

```text
Left side  = reverse
Right side = forward
```

Rotate right:

```text
Left side  = forward
Right side = reverse
```

Stop:

```text
All motors = 0
```

Add per-motor direction inversion:

```cpp
FL_INVERT
FR_INVERT
RL_INVERT
RR_INVERT
```

This allows physical motor wiring to be corrected in software.

---

# 14. PWM

Use the ESP32 Arduino PWM API supported by the installed ESP32 Arduino Core.

Do not assume AVR-specific functions or timer APIs.

If using the newer ESP32 Arduino core, prefer the current `analogWrite()` interface where appropriate.

If explicit LEDC configuration is required by the selected core/API, isolate it inside `motor_controller.cpp`.

The rest of the project must not depend on the PWM implementation.

---

# 15. Obstacle Safety

Initial threshold:

```text
30 cm
```

When moving forward:

```text
distance > 30 cm
        ↓
continue
```

When:

```text
distance <= 30 cm
        ↓
STOP FORWARD MOTION
```

Turning, rotation, backward movement, and STOP may remain available.

Recommended hysteresis:

```text
Stop threshold  = 30 cm
Clear threshold = 35 cm
```

The ESP32 must perform this check locally.

The phone or AI must never be responsible for the final obstacle-safety decision.

---

# 16. Emergency Stop

Implement a software emergency-stop state.

STOP must:

1. Set all motor outputs to zero.
2. Clear active movement.
3. Enter stopped state.
4. Wait for a new valid movement command.

On power-up:

```text
ALL MOTORS STOPPED
```

The robot must never automatically start moving after boot.

---

# 17. Wi-Fi Phone Control

The ESP32 should initially operate as a Wi-Fi Access Point.

Suggested:

```text
SSID:
RobotPrototype
```

Password must be stored in `config.h`.

Use a fixed Wi-Fi channel so ESP-NOW can coexist reliably.

Suggested address:

```text
192.168.4.1
```

Create a simple web interface.

Controls:

```text
FORWARD
BACKWARD
LEFT
RIGHT
ROTATE LEFT
ROTATE RIGHT
STOP
```

Also show:

```text
Speed
Distance
Obstacle status
Active controller
ESP32-CAM status
Robot state
```

Suggested endpoints:

```text
GET  /
GET  /api/status
POST /api/command
POST /api/stop
POST /api/speed
```

Use the ESP32 Arduino `WebServer` class unless another installed core API is required.

HTTP handlers must not directly manipulate motors.

---

# 18. Wi-Fi Heartbeat

The phone control interface should periodically send a heartbeat.

Recommended interval:

```text
500 ms
```

The ESP32 should track:

```cpp
lastWifiHeartbeat
```

If:

```text
millis() - lastWifiHeartbeat > COMMAND_TIMEOUT_MS
```

the phone is considered unavailable.

When the active controller times out:

```text
fallback to next valid controller
```

If no controller is valid:

```text
STOP
```

---

# 19. Bluetooth

Bluetooth is a fallback communication source.

If implemented:

- Use the ESP32 Arduino Bluetooth/BLE APIs.
- Use the same `RobotCommand` format.
- Use heartbeat timeout.
- Do not duplicate motor-control logic.
- Feed Bluetooth commands into the same Control Manager.

If reliable simultaneous Wi-Fi + ESP-NOW + Bluetooth operation is not supported by the selected ESP32 core/configuration, document the limitation and prioritize reliable Wi-Fi + ESP-NOW operation.

---

# 20. ESP-NOW

Use ESP-NOW between:

```text
ESP32 main controller
        ↕
ESP32-CAM
```

The ESP32-CAM sends a heartbeat every:

```text
500 ms
```

The main ESP32 tracks:

```cpp
lastESPNowHeartbeat
```

Timeout:

```text
1500 ms
```

Use the same Wi-Fi channel for the ESP32 AP and ESP-NOW.

Prefer unicast ESP-NOW once the peer MAC address is known.

---

# 21. ESP-NOW Packet

Use a compact structure:

```cpp
struct RobotPacket {
    uint32_t magic;
    uint8_t version;
    uint8_t source;
    uint8_t command;
    uint8_t speed;
    uint32_t sequence;
    uint32_t timestamp;
};
```

Validate:

- packet size
- magic number
- protocol version
- source
- command
- sequence where appropriate

Reject invalid packets.

Do not cast arbitrary network data into a structure without validating the received length.

---

# 22. ESP32-CAM Arduino Sketch

Create a separate Arduino sketch:

```text
ESP32CAMFallback/
└── ESP32CAMFallback.ino
```

Initial responsibilities:

1. Boot.
2. Print MAC address.
3. Initialize Wi-Fi.
4. Initialize ESP-NOW.
5. Register the main ESP32 as peer.
6. Send heartbeat every 500 ms.
7. Accept serial test commands.
8. Send test commands to the main ESP32.
9. Initialize camera if the selected ESP32-CAM board configuration permits it.
10. Do not perform CV yet.

---

# 23. ESP32-CAM Test Commands

Serial commands:

```text
f → FORWARD
b → BACKWARD
l → LEFT
r → RIGHT
q → ROTATE_LEFT
e → ROTATE_RIGHT
s → STOP
```

These are transmitted through ESP-NOW.

---

# 24. Main ESP32 Serial Test Commands

Support:

```text
f → forward
b → backward
l → left
r → right
q → rotate left
e → rotate right
s → stop
+ → increase speed
- → decrease speed
```

This allows complete motor testing without the phone.

---

# 25. Non-Blocking Firmware

Do not build the firmware around long `delay()` calls.

Use `millis()` timers.

Main loop should conceptually be:

```cpp
void loop() {
    updateWiFi();
    updateBluetooth();
    updateESPNow();

    updateUltrasonic();
    updateSafety();

    updateControlSource();
    processCommands();

    updateMotors();
    updateStatus();

    handleWebServer();
}
```

Subsystems must remain responsive.

Avoid blocking loops.

Short delays may be used only where unavoidable during initialization.

---

# 26. Configuration

Put configuration in `config.h`.

Example:

```cpp
#define FL_IN1 16
#define FL_IN2 17

#define FR_IN1 18
#define FR_IN2 19

#define RL_IN1 21
#define RL_IN2 22

#define RR_IN1 23
#define RR_IN2 25

#define ULTRASONIC_TRIG 26
#define ULTRASONIC_ECHO 27

#define OBSTACLE_STOP_CM 30
#define OBSTACLE_CLEAR_CM 35

#define HEARTBEAT_INTERVAL_MS 500
#define COMMAND_TIMEOUT_MS 1500

#define DEFAULT_SPEED 150
#define MAX_SPEED 255

#define FL_INVERT false
#define FR_INVERT false
#define RL_INVERT false
#define RR_INVERT false
```

Prefer `constexpr` or typed constants when appropriate.

Do not hardcode these values throughout the project.

---

# 27. Robot State

Maintain a central state structure.

Example:

```cpp
struct RobotState {
    uint8_t activeSource;
    uint8_t currentCommand;
    uint8_t speed;

    float distanceCm;
    bool obstacleDetected;

    bool wifiAlive;
    bool bluetoothAlive;
    bool espNowAlive;

    bool emergencyStop;
    bool fault;

    uint32_t uptime;
};
```

All subsystems should report state to the central state manager.

---

# 28. Telemetry

Expose status as JSON.

Example:

```json
{
  "source": "PHONE_WIFI",
  "command": "FORWARD",
  "speed": 150,
  "distance_cm": 72,
  "obstacle": false,
  "espnow_connected": true,
  "wifi_connected": true,
  "bluetooth_connected": false,
  "uptime_ms": 123456,
  "fault": false
}
```

Web UI can poll:

```text
/api/status
```

every:

```text
250–500 ms
```

---

# 29. Serial Logging

Use:

```cpp
Serial.begin(115200);
```

Log important events:

- boot
- firmware version
- MAC address
- Wi-Fi AP startup
- ESP-NOW startup
- ESP32-CAM online/offline
- active controller changes
- commands
- obstacle detection
- safety stops
- configuration errors

Do not print continuously at high frequency.

Avoid flooding the serial monitor with ultrasonic readings.

---

# 30. Arduino IDE Setup

Install Arduino IDE 2.x.

Add the ESP32 board package through:

```text
File → Preferences
```

Add the official Espressif ESP32 board package URL supported by the current ESP32 Arduino Core release.

Then:

```text
Tools → Board → Boards Manager
```

Install:

```text
esp32 by Espressif Systems
```

Select the correct board.

For the main controller, select the appropriate ESP32 DevKit board matching the physical board.

For the ESP32-CAM, select the board definition matching the actual module.

Do not blindly select `AI Thinker ESP32-CAM` unless the physical module is actually that board.

---

# 31. Arduino CLI Setup

The same sketches must compile through Arduino CLI.

Example:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

List boards:

```bash
arduino-cli board list
```

Compile:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 RobotPrototype
```

Upload:

```bash
arduino-cli upload -p COMx --fqbn esp32:esp32:esp32 RobotPrototype
```

Replace:

```text
COMx
```

with the actual ESP32 serial port.

For ESP32-CAM, use the correct FQBN for the selected board definition.

Do not hardcode a board FQBN into the source code.

---

# 32. Development Order

Do not implement the complete system simultaneously.

Follow this order.

## Phase 1: One Motor

Test:

```text
ESP32
  ↓
DRV8833
  ↓
Motor
```

Verify:

- forward
- reverse
- stop
- PWM speed

---

## Phase 2: Four Motors

Test every motor individually:

```text
FL
FR
RL
RR
```

Then test:

```text
FORWARD
BACKWARD
LEFT
RIGHT
ROTATE_LEFT
ROTATE_RIGHT
STOP
```

Fix direction inversion before continuing.

---

## Phase 3: Ultrasonic

Verify:

```text
distance measurement
```

Then:

```text
FORWARD + obstacle
        ↓
      STOP
```

---

## Phase 4: Phone Wi-Fi

Connect the phone to:

```text
RobotPrototype
```

Open:

```text
192.168.4.1
```

Test every control.

---

## Phase 5: Timeout

Move the robot.

Disconnect the phone.

Expected:

```text
Phone heartbeat disappears
        ↓
Timeout
        ↓
Fallback / STOP
```

---

## Phase 6: ESP32-CAM

Verify:

```text
ESP32-CAM
    ↓
ESP-NOW heartbeat
    ↓
Main ESP32
```

Then test ESP-NOW movement commands.

---

## Phase 7: Automatic Failover

Test:

```text
Phone connected
    ↓
Phone controls robot

Phone disconnected
    ↓
ESP32-CAM controls robot

ESP32-CAM disconnected
    ↓
Robot stops
```

---

## Phase 8: Stop

Do not add AI until every previous phase is reliable.

---

# 33. Future Phone AI Architecture

After the physical prototype works:

```text
                     PHONE
              ┌──────────────────┐
              │ Camera           │
              │ Gesture CV       │
              │ Person tracking  │
              │ Voice commands   │
              │ LLM              │
              │ Robot UI         │
              └────────┬─────────┘
                       │
                      Wi-Fi
                       │
                       ▼
                    ESP32
                       │
              Safety + Motors
                       │
                       ▼
                    Robot
```

The phone can later perform:

- gesture recognition
- person detection
- person following
- voice commands
- natural-language commands
- camera processing
- UI
- telemetry
- robot expressions

The ESP32 remains responsible for:

- motors
- obstacle safety
- sensor processing
- deterministic control
- communication
- emergency stop

---

# 34. AI Command Rules

When AI is added, AI produces high-level commands.

Example:

```json
{
  "action": "FORWARD",
  "speed": 150
}
```

or:

```json
{
  "action": "ROTATE_LEFT",
  "speed": 120
}
```

AI must never directly control:

```text
GPIO
PWM
DRV8833
motor outputs
```

All AI-generated commands must pass through:

```text
Command Parser
      ↓
Control Manager
      ↓
Safety Manager
      ↓
Motor Controller
```

---

# 35. Power Rules

Do not finalize the battery configuration until the actual BO motor voltage and current requirements are known.

Important:

- Never power motors from ESP32 3.3 V.
- Motor power goes to DRV8833 VM.
- ESP32 needs an appropriate regulated supply.
- All grounds must be common.
- Do not connect four 18650 cells in an arbitrary series/parallel arrangement.
- Do not assume the 3.7 V 800 mAh battery can safely drive four motors.
- TP4056 modules are generally intended for single-cell Li-ion charging, not arbitrary multi-cell series packs.
- Use proper protection/BMS and an appropriate power architecture for the final battery system.

For initial testing, use a current-limited bench supply or a verified suitable motor supply.

---

# 36. Definition of Done

The first prototype is complete when:

- [ ] Arduino IDE compilation works
- [ ] Arduino CLI compilation works
- [ ] One motor works
- [ ] Four motors work
- [ ] Forward works
- [ ] Backward works
- [ ] Left works
- [ ] Right works
- [ ] Rotation works
- [ ] Stop works
- [ ] PWM speed works
- [ ] Ultrasonic distance works
- [ ] Obstacle safety works
- [ ] Wi-Fi AP works
- [ ] Phone web UI works
- [ ] Phone heartbeat works
- [ ] Command timeout works
- [ ] ESP32-CAM boots
- [ ] ESP32-CAM sends ESP-NOW heartbeat
- [ ] Main ESP32 receives heartbeat
- [ ] ESP32-CAM movement commands work
- [ ] Phone → ESP32 control works
- [ ] Phone timeout triggers fallback
- [ ] ESP32-CAM timeout causes STOP
- [ ] Robot boots with motors stopped
- [ ] No remote command can bypass local safety
- [ ] No PlatformIO dependency exists

Only after all of these work should AI/CV development begin.
