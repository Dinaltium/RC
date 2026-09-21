/**
 * RobotPrototype.ino — Main ESP32 Robot Controller
 *
 * Entry point for the 4-wheel ESP32 robot prototype.
 * Boots with all motors stopped. Non-blocking main loop.
 *
 * Architecture:
 *   Communication → Command Parser → Control Manager → Safety Manager → Motor Controller
 *
 * No remote source may bypass local safety checks.
 */

#include "config.h"
#include "command.h"
#include "robot_state.h"
#include "motor_controller.h"
#include "ultrasonic.h"
#include "tof.h"
#include "bump.h"
#include "cliff.h"
#include "safety_manager.h"
#include "command_parser.h"
#include "control_manager.h"
#include "wifi_control.h"
#include "espnow_control.h"
#include "bluetooth_control.h"
#include "web_server.h"
#include <WiFi.h>

// ============================================================
// Serial debug command processing
// ============================================================
static uint8_t serialSpeed = DEFAULT_SPEED;

static void processSerialInput() {
    while (Serial.available()) {
        char c = Serial.read();

        // Speed adjustment
        if (c == '+') {
            if (serialSpeed <= MAX_SPEED - SPEED_STEP) {
                serialSpeed += SPEED_STEP;
            } else {
                serialSpeed = MAX_SPEED;
            }
            Serial.print(F("[SERIAL] Speed: "));
            Serial.println(serialSpeed);
            stateSetSpeed(serialSpeed);
            return;
        }
        if (c == '-') {
            if (serialSpeed >= SPEED_STEP) {
                serialSpeed -= SPEED_STEP;
            } else {
                serialSpeed = 0;
            }
            Serial.print(F("[SERIAL] Speed: "));
            Serial.println(serialSpeed);
            stateSetSpeed(serialSpeed);
            return;
        }

        // Movement commands
        RobotCommand cmd;
        if (parseSerialChar(c, SRC_SERIAL_DEBUG, serialSpeed, cmd)) {
            Serial.print(F("[SERIAL] Command: "));
            Serial.println(actionName(cmd.action));
            controlSubmitCommand(cmd);
        }
    }
}

// ============================================================
// Status logging (periodic, not every loop)
// ============================================================
static uint32_t lastStatusLog = 0;

static void logStatus() {
    uint32_t now = millis();
    if (now - lastStatusLog < STATUS_INTERVAL_MS) return;
    lastStatusLog = now;

    RobotState& st = stateGet();
    Serial.print(F("[STATUS] src="));
    Serial.print(sourceName(st.activeSource));
    Serial.print(F(" cmd="));
    Serial.print(actionName(st.currentCommand));
    Serial.print(F(" spd="));
    Serial.print(st.speed);
    Serial.print(F(" dist="));
    Serial.print(st.distanceCm, 1);
    Serial.print(F("cm obs="));
    Serial.print(st.obstacleDetected ? "Y" : "N");
    Serial.print(F(" bump="));
    Serial.print(st.bumpFront ? "F" : "-");
    Serial.print(st.bumpRear ? "R" : "-");
    Serial.print(F(" cliff="));
    Serial.print(st.cliffFront ? "F" : "-");
    Serial.print(st.cliffRear ? "R" : "-");
    Serial.print(F(" wifi="));
    Serial.print(st.wifiAlive ? "Y" : "N");
    Serial.print(F(" espnow="));
    Serial.print(st.espNowAlive ? "Y" : "N");
    Serial.print(F(" up="));
    Serial.print(now / 1000);
    Serial.println(F("s"));
}

// ============================================================
// setup() — runs once at boot
// ============================================================
void setup() {
    // Serial
    Serial.begin(SERIAL_BAUD);
    delay(100);  // brief delay for serial stability

    Serial.println(F("========================================"));
    Serial.println(F("  Robot Prototype"));
    Serial.print(F("  Firmware: "));
    Serial.println(FIRMWARE_VERSION);
    Serial.println(F("========================================"));

    // Initialize subsystems in order
    stateInit();
    motorInit();         // all motors stopped on boot
    tofInit();           // I2C ToF sensors (front range and/or cliff pair)
    ultrasonicInit();    // front range in whichever mode config.h selects
    bumpInit();
    cliffInit();
    safetyInit();
    controlInit();

    // Communication
    wifiInit();
#ifdef ENABLE_ESPNOW
    espnowInit();
#else
    Serial.println(F("[ESPNOW] Disabled (ENABLE_ESPNOW not defined) — ESP32-CAM fallback off"));
#endif
    bluetoothInit();

    // Web server (after Wi-Fi is up)
    webServerInit();

    // Print MAC for ESP-NOW peer setup
    Serial.print(F("[BOOT] MAC Address: "));
    Serial.println(WiFi.macAddress());
    Serial.println(F("[BOOT] Ready — all motors stopped"));
    Serial.println(F("[BOOT] Serial commands: f/b/l/r/q/e/s, !/x (E-Stop), c (Clear E-Stop), +/- (speed)"));
    Serial.println(F("========================================"));
}

// ============================================================
// loop() — non-blocking main loop
// ============================================================
void loop() {
    // 1. Process communication inputs
    processSerialInput();
    bluetoothUpdate();
#ifdef ENABLE_ESPNOW
    espnowUpdate();  // Decoupled packet queue processing
#endif

    // 2. Update sensors
    tofUpdate();
    ultrasonicUpdate();
    bumpUpdate();
    cliffUpdate();

    // 3. Update safety
    safetyUpdate();

    // 4. Update control source / failover
    controlUpdate();

    // 5. Update state
    stateUpdateUptime();

    // 6. Handle web server
    webServerHandle();

    // 7. Periodic status log
    logStatus();
}
