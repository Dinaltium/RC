/**
 * config.h — Central configuration for Robot Prototype
 *
 * All GPIO assignments, timing constants, and network settings
 * are defined here. Do not hardcode these values elsewhere.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// ============================================================
// Firmware Version
// ============================================================
#define FIRMWARE_VERSION "1.0.0"

// ============================================================
// Motor GPIO — DRV8833 Connections (one motor per driver)
// ============================================================
// Front Left — DRV8833 #1
constexpr uint8_t FL_IN1 = 16;
constexpr uint8_t FL_IN2 = 17;

// Front Right — DRV8833 #2
constexpr uint8_t FR_IN1 = 18;
constexpr uint8_t FR_IN2 = 19;

// Rear Left — DRV8833 #3
constexpr uint8_t RL_IN1 = 21;
constexpr uint8_t RL_IN2 = 22;

// Rear Right — DRV8833 #4
constexpr uint8_t RR_IN1 = 23;
constexpr uint8_t RR_IN2 = 25;

// ============================================================
// Motor Direction Inversion
// Set true if a motor spins opposite to expected direction
// ============================================================
constexpr bool FL_INVERT = false;
constexpr bool FR_INVERT = false;
constexpr bool RL_INVERT = false;
constexpr bool RR_INVERT = false;

// ============================================================
// PWM Configuration
// ============================================================
constexpr uint8_t  PWM_RESOLUTION_BITS = 8;       // 0–255
constexpr uint32_t PWM_FREQUENCY        = 1000;    // 1 kHz

// LEDC channels (used only if explicit LEDC API is needed)
constexpr uint8_t LEDC_CH_FL_IN1 = 0;
constexpr uint8_t LEDC_CH_FL_IN2 = 1;
constexpr uint8_t LEDC_CH_FR_IN1 = 2;
constexpr uint8_t LEDC_CH_FR_IN2 = 3;
constexpr uint8_t LEDC_CH_RL_IN1 = 4;
constexpr uint8_t LEDC_CH_RL_IN2 = 5;
constexpr uint8_t LEDC_CH_RR_IN1 = 6;
constexpr uint8_t LEDC_CH_RR_IN2 = 7;

// ============================================================
// Ultrasonic Sensor — HC-SR04
// ECHO requires voltage divider (5 V → 3.3 V safe)
// ============================================================
constexpr uint8_t ULTRASONIC_TRIG = 26;
constexpr uint8_t ULTRASONIC_ECHO = 27;

// ============================================================
// Obstacle Safety Thresholds (cm)
// Hysteresis: stop at STOP, resume at CLEAR
// ============================================================
constexpr float OBSTACLE_STOP_CM  = 30.0f;
constexpr float OBSTACLE_CLEAR_CM = 35.0f;

// ============================================================
// Speed Defaults
// ============================================================
constexpr uint8_t DEFAULT_SPEED = 150;
constexpr uint8_t MAX_SPEED     = 255;
constexpr uint8_t SPEED_STEP    = 10;   // for +/- serial commands
constexpr uint8_t TURN_SPEED_REDUCTION = 60; // inner wheels slow by this

// ============================================================
// Heartbeat & Timeout (ms)
// ============================================================
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
constexpr uint32_t COMMAND_TIMEOUT_MS    = 1500;

// ============================================================
// Ultrasonic Measurement Interval (ms)
// ============================================================
constexpr uint32_t ULTRASONIC_INTERVAL_MS = 60;
constexpr uint32_t ULTRASONIC_TIMEOUT_US  = 30000; // ~5 m max

// ============================================================
// Status Update Interval (ms) — for serial logging
// ============================================================
constexpr uint32_t STATUS_INTERVAL_MS = 2000;

// ============================================================
// Wi-Fi Access Point Settings
// ============================================================
#define WIFI_SSID     "RobotPrototype"
#define WIFI_PASSWORD "robot1234"
constexpr uint8_t WIFI_CHANNEL = 1;  // fixed channel for ESP-NOW coexistence

// ============================================================
// ESP-NOW Protocol
// ============================================================
constexpr uint32_t ESPNOW_MAGIC   = 0x524F424F; // "ROBO" in ASCII
constexpr uint8_t  ESPNOW_VERSION = 1;

// ESP32-CAM peer MAC address
// Flash the CAM sketch first, read its MAC from serial, then update here.
// When left as all 0xFF (broadcast) or all 0x00, development mode is active (with warnings).
constexpr uint8_t ESP32_CAM_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Enforce strict sender MAC verification against ESP32_CAM_MAC
constexpr bool STRICT_SENDER_MAC_CHECK = true;

inline bool isCamMacConfigured() {
    bool allFF = true, all00 = true;
    for (int i = 0; i < 6; i++) {
        if (ESP32_CAM_MAC[i] != 0xFF) allFF = false;
        if (ESP32_CAM_MAC[i] != 0x00) all00 = false;
    }
    return !allFF && !all00;
}

// ============================================================
// Bluetooth (compile-guarded)
// Uncomment the line below to enable Bluetooth Serial fallback.
// Note: simultaneous Wi-Fi AP + ESP-NOW + BT may be unreliable.
// ============================================================
// #define ENABLE_BLUETOOTH
#define BT_DEVICE_NAME "RobotPrototype"

// ============================================================
// Serial
// ============================================================
constexpr uint32_t SERIAL_BAUD = 115200;

#endif // CONFIG_H
