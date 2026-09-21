/**
 * config.h — ESP32-CAM (AI Thinker) as the robot's camera
 *
 * The board joins the robot's Wi-Fi AP as a station with a fixed address
 * and serves its camera over HTTP:
 *     http://192.168.4.20/stream   MJPEG, what the fly host consumes
 *     http://192.168.4.20/capture  one JPEG
 *     http://192.168.4.20/         status JSON
 *
 * Host side:  ./run.sh --camera http://192.168.4.20/stream ...
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

#define FIRMWARE_VERSION "2.0.0"
constexpr uint32_t SERIAL_BAUD = 115200;

// ============================================================
// Board — AI Thinker ESP32-CAM (OV2640, flash LED on GPIO 4)
// ============================================================
#define CAMERA_MODEL_AI_THINKER
constexpr int FLASH_LED_PIN = 4;

// ============================================================
// Wi-Fi — join the robot's access point (must match RobotPrototype/config.h)
// ============================================================
#define WIFI_SSID     "RobotPrototype"
#define WIFI_PASSWORD "robot1234"
constexpr uint8_t WIFI_CHANNEL = 1;

// Fixed address inside the robot's 192.168.4.0/24 network. The ESP32 AP
// hands out .2 upwards by DHCP; .20 stays clear of that.
constexpr uint8_t CAM_IP[4]      = {192, 168, 4, 20};
constexpr uint8_t CAM_GATEWAY[4] = {192, 168, 4, 1};
constexpr uint8_t CAM_SUBNET[4]  = {255, 255, 255, 0};
constexpr uint32_t WIFI_RETRY_MS = 5000;

// ============================================================
// Camera
// ============================================================
// QVGA (320x240) is plenty: the retina downsamples to 64x48. Larger frames
// only cost Wi-Fi bandwidth and phone CPU for JPEG decode.
#define CAM_FRAME_SIZE   FRAMESIZE_QVGA
constexpr int CAM_JPEG_QUALITY = 12;    // 0 (best) .. 63; 10-14 is a good MJPEG range
constexpr int CAM_FB_COUNT     = 2;     // double buffer when PSRAM is present
constexpr int CAM_FLIP_V       = 0;     // 1 if the module is mounted upside down
constexpr int CAM_MIRROR_H     = 0;
constexpr uint32_t STREAM_MIN_FRAME_MS = 66;   // cap ~15 fps; the fly ticks at 5-10 Hz

// ============================================================
// ESP-NOW fallback controller (optional, off by default)
// Enable together with ENABLE_ESPNOW in RobotPrototype/config.h to let the
// CAM send heartbeats / serial test commands to the robot over ESP-NOW.
// ============================================================
// #define ENABLE_ESPNOW_FALLBACK
constexpr uint32_t ESPNOW_MAGIC   = 0x524F424F; // "ROBO"
constexpr uint8_t  ESPNOW_VERSION = 1;
constexpr uint8_t  MAIN_ESP32_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
constexpr uint8_t  DEFAULT_SPEED = 150;

#endif // CONFIG_H
