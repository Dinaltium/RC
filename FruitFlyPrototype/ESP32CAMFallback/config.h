/**
 * config.h — Configuration for ESP32-CAM Fallback Controller
 *
 * Must match the main ESP32's ESP-NOW settings.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// ============================================================
// Firmware
// ============================================================
#define FIRMWARE_VERSION "1.0.0"

// ============================================================
// Serial
// ============================================================
constexpr uint32_t SERIAL_BAUD = 115200;

// ============================================================
// Camera Board Model Selection
// Explicitly define your physical module model.
// Default: AI Thinker ESP32-CAM
// ============================================================
#define CAMERA_MODEL_AI_THINKER
// #define CAMERA_MODEL_WROVER_KIT
// #define CAMERA_MODEL_ESP_EYE

// ============================================================
// Wi-Fi Channel — MUST match main ESP32 AP channel
// ============================================================
constexpr uint8_t WIFI_CHANNEL = 1;

// ============================================================
// ESP-NOW Protocol — MUST match main ESP32
// ============================================================
constexpr uint32_t ESPNOW_MAGIC   = 0x524F424F; // "ROBO"
constexpr uint8_t  ESPNOW_VERSION = 1;

// ============================================================
// Main ESP32 peer MAC address
// Flash the main ESP32 sketch first, read its MAC from serial,
// then update this array.
// Use broadcast (0xFF) as default until MAC is known.
// ============================================================
constexpr uint8_t MAIN_ESP32_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================
// Heartbeat interval (ms)
// ============================================================
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;

// ============================================================
// Default speed for test commands
// ============================================================
constexpr uint8_t DEFAULT_SPEED = 150;

#endif // CONFIG_H
