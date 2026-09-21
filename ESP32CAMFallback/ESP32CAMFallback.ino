/**
 * ESP32CAMFallback.ino — ESP32-CAM Fallback Controller
 *
 * Responsibilities:
 *   1. Boot and print MAC address
 *   2. Initialize Wi-Fi (station mode, fixed channel)
 *   3. Initialize ESP-NOW
 *   4. Register main ESP32 as peer
 *   5. Send heartbeat every 500 ms
 *   6. Accept serial test commands (f/b/l/r/q/e/s)
 *   7. Send test commands to main ESP32 via ESP-NOW
 *   8. Initialize camera if board supports it
 *   9. No CV processing in this prototype
 */

#include "config.h"
#include "espnow_control.h"
#include <WiFi.h>
#include <Arduino.h>

// Explicit board camera initialization (AI Thinker model)
#if defined(CAMERA_MODEL_AI_THINKER)
#include "esp_camera.h"
static bool cameraAvailable = false;

// AI Thinker ESP32-CAM pin definitions
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static void initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.print(F("[CAMERA] Init failed: 0x"));
        Serial.println(err, HEX);
        cameraAvailable = false;
    } else {
        Serial.println(F("[CAMERA] Initialized (no CV in this prototype)"));
        cameraAvailable = true;
    }
}
#else
static void initCamera() {
    Serial.println(F("[CAMERA] Camera support not available for this board config"));
}
#endif

// ============================================================
// Heartbeat timing
// ============================================================
static uint32_t lastHeartbeat = 0;

// ============================================================
// Serial test command processing
// ============================================================
static void processSerialInput() {
    while (Serial.available()) {
        char c = Serial.read();
        uint8_t cmd = 0xFF;

        switch (c) {
            case 'f': case 'F': cmd = CMD_FORWARD;      break;
            case 'b': case 'B': cmd = CMD_BACKWARD;     break;
            case 'l': case 'L': cmd = CMD_LEFT;         break;
            case 'r': case 'R': cmd = CMD_RIGHT;        break;
            case 'q': case 'Q': cmd = CMD_ROTATE_LEFT;  break;
            case 'e': case 'E': cmd = CMD_ROTATE_RIGHT; break;
            case 's': case 'S': cmd = CMD_STOP;            break;
            case '!': case 'x': case 'X': cmd = CMD_EMERGENCY_STOP; break;
            case 'c': case 'C': cmd = CMD_CLEAR_EMERGENCY; break;
            default: continue;
        }

        if (cmd != 0xFF) {
            Serial.print(F("[SERIAL] Sending command: "));
            Serial.println(c);
            if (espnowCamSendCommand(cmd, DEFAULT_SPEED)) {
                Serial.println(F("[SERIAL] Sent OK"));
            } else {
                Serial.println(F("[SERIAL] Send FAILED"));
            }
        }
    }
}

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);

    Serial.println(F("========================================"));
    Serial.println(F("  ESP32-CAM Fallback Controller"));
    Serial.print(F("  Firmware: "));
    Serial.println(FIRMWARE_VERSION);
    Serial.println(F("========================================"));

    // Print MAC address (needed for main ESP32 config.h)
    WiFi.mode(WIFI_STA);
    Serial.print(F("[BOOT] MAC Address: "));
    Serial.println(WiFi.macAddress());

    // Initialize ESP-NOW
    espnowCamInit();

    // Attempt camera init
    initCamera();

    Serial.println(F("[BOOT] Ready"));
    Serial.println(F("[BOOT] Serial commands: f/b/l/r/q/e/s, !/x (E-Stop), c (Clear E-Stop)"));
    Serial.println(F("========================================"));
}

// ============================================================
// loop() — non-blocking
// ============================================================
void loop() {
    // Send heartbeat at interval
    uint32_t now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        espnowCamSendHeartbeat();
    }

    // Process serial test commands
    processSerialInput();
}
