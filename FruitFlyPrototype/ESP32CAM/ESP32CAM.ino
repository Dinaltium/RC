/**
 * ESP32CAM.ino — robot-mounted camera for the FruitFly host
 *
 *   1. Init OV2640 (AI Thinker pin map), JPEG QVGA.
 *   2. Join the robot's AP "RobotPrototype" as a station, fixed IP .20.
 *   3. Serve /stream (MJPEG), /capture (JPEG), / (status) on port 80.
 *   4. Optionally act as the ESP-NOW fallback controller (ENABLE_ESPNOW_FALLBACK).
 *
 * The board carries no robot logic: it is a camera on the network. The fly
 * host on the phone or laptop pulls frames with --camera http://192.168.4.20/stream
 *
 * Board: "AI Thinker ESP32-CAM"  (arduino-cli fqbn esp32:esp32:esp32cam)
 * Flash: GPIO 0 to GND during reset (or the programmer's IO0 button), 5 V supply.
 */

#include "config.h"
#include "espnow_control.h"
#include <WiFi.h>
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ------------------------------------------------------------------
// AI Thinker pin map
// ------------------------------------------------------------------
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

static bool     cameraReady   = false;
static uint32_t framesServed  = 0;
static uint32_t lastWifiTry   = 0;
static httpd_handle_t httpd   = nullptr;

// ------------------------------------------------------------------
// Camera
// ------------------------------------------------------------------
static bool initCamera() {
    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer   = LEDC_TIMER_0;
    c.pin_d0 = Y2_GPIO_NUM;  c.pin_d1 = Y3_GPIO_NUM;  c.pin_d2 = Y4_GPIO_NUM;  c.pin_d3 = Y5_GPIO_NUM;
    c.pin_d4 = Y6_GPIO_NUM;  c.pin_d5 = Y7_GPIO_NUM;  c.pin_d6 = Y8_GPIO_NUM;  c.pin_d7 = Y9_GPIO_NUM;
    c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
    c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
    c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
    c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size   = CAM_FRAME_SIZE;
    c.jpeg_quality = CAM_JPEG_QUALITY;
    c.fb_count     = psramFound() ? CAM_FB_COUNT : 1;
    c.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    c.grab_mode    = CAMERA_GRAB_LATEST;   // always serve the newest frame, never a stale one

    esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        Serial.print(F("[CAMERA] init failed: 0x")); Serial.println(err, HEX);
        return false;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, CAM_FLIP_V);
        s->set_hmirror(s, CAM_MIRROR_H);
        s->set_brightness(s, 0);
        s->set_saturation(s, 0);
        // Auto exposure/gain on: the retina works on relative change, not
        // absolute brightness, so let the sensor keep the histogram sane.
        s->set_exposure_ctrl(s, 1);
        s->set_gain_ctrl(s, 1);
        s->set_whitebal(s, 1);
    }
    Serial.print(F("[CAMERA] ready, PSRAM ")); Serial.println(psramFound() ? F("yes") : F("no"));
    return true;
}

// ------------------------------------------------------------------
// Wi-Fi station on the robot's AP
// ------------------------------------------------------------------
static void wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                       // latency matters more than power here
    WiFi.config(IPAddress(CAM_IP), IPAddress(CAM_GATEWAY), IPAddress(CAM_SUBNET));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
    lastWifiTry = millis();
    Serial.print(F("[WIFI] joining ")); Serial.println(WIFI_SSID);
}

static void wifiMaintain() {
    static bool wasUp = false;
    bool up = WiFi.status() == WL_CONNECTED;
    if (up && !wasUp) {
        Serial.print(F("[WIFI] up  ")); Serial.print(WiFi.localIP());
        Serial.print(F("  rssi ")); Serial.println(WiFi.RSSI());
        Serial.print(F("[WIFI] stream http://")); Serial.print(WiFi.localIP()); Serial.println(F("/stream"));
    }
    if (!up && wasUp) Serial.println(F("[WIFI] lost, retrying"));
    if (!up && millis() - lastWifiTry > WIFI_RETRY_MS) {
        WiFi.disconnect();
        wifiConnect();
    }
    wasUp = up;
}

// ------------------------------------------------------------------
// HTTP
// ------------------------------------------------------------------
static const char* STREAM_CT   = "multipart/x-mixed-replace;boundary=frame";
static const char* STREAM_SEP  = "\r\n--frame\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t streamHandler(httpd_req_t* req) {
    if (!cameraReady) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, STREAM_CT);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    char part[64];
    uint32_t lastFrame = 0;
    while (true) {
        uint32_t now = millis();
        if (now - lastFrame < STREAM_MIN_FRAME_MS) { delay(STREAM_MIN_FRAME_MS - (now - lastFrame)); }
        lastFrame = millis();
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { Serial.println(F("[CAMERA] fb_get failed")); return ESP_FAIL; }
        int n = snprintf(part, sizeof(part), STREAM_PART, (unsigned)fb->len);
        esp_err_t r = httpd_resp_send_chunk(req, STREAM_SEP, strlen(STREAM_SEP));
        if (r == ESP_OK) r = httpd_resp_send_chunk(req, part, n);
        if (r == ESP_OK) r = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
        esp_camera_fb_return(fb);
        if (r != ESP_OK) break;         // client went away
        framesServed++;
    }
    return ESP_OK;
}

static esp_err_t captureHandler(httpd_req_t* req) {
    if (!cameraReady) return httpd_resp_send_500(req);
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_send(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    framesServed++;
    return r;
}

static esp_err_t statusHandler(httpd_req_t* req) {
    char body[192];
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"camera\":%s,\"frames\":%u,\"rssi\":%d,\"psram\":%s,\"uptime_s\":%u,\"fw\":\"%s\"}",
             cameraReady ? "true" : "false", (unsigned)framesServed, WiFi.RSSI(),
             psramFound() ? "true" : "false", (unsigned)(millis() / 1000), FIRMWARE_VERSION);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, body, strlen(body));
}

static void startHttp() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_open_sockets = 4;     // one streamer + a browser + slack
    cfg.lru_purge_enable = true;
    if (httpd_start(&httpd, &cfg) != ESP_OK) { Serial.println(F("[HTTP] start failed")); return; }
    httpd_uri_t stream  = { "/stream",  HTTP_GET, streamHandler,  nullptr };
    httpd_uri_t capture = { "/capture", HTTP_GET, captureHandler, nullptr };
    httpd_uri_t status  = { "/",        HTTP_GET, statusHandler,  nullptr };
    httpd_register_uri_handler(httpd, &stream);
    httpd_register_uri_handler(httpd, &capture);
    httpd_register_uri_handler(httpd, &status);
    Serial.println(F("[HTTP] /stream /capture / on port 80"));
}

// ------------------------------------------------------------------
// Optional ESP-NOW fallback: serial letters become robot commands
// ------------------------------------------------------------------
#ifdef ENABLE_ESPNOW_FALLBACK
static uint32_t lastHeartbeat = 0;
static void processSerialInput() {
    while (Serial.available()) {
        char ch = Serial.read();
        uint8_t cmd = 0xFF;
        switch (ch) {
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
        Serial.print(F("[SERIAL] ")); Serial.print(ch);
        Serial.println(espnowCamSendCommand(cmd, DEFAULT_SPEED) ? F(" sent") : F(" FAILED"));
    }
}
#endif

// ------------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);      // the flash LED is blinding and wastes 100 mA

    Serial.println(F("========================================"));
    Serial.println(F("  ESP32-CAM robot camera"));
    Serial.print(F("  Firmware: ")); Serial.println(FIRMWARE_VERSION);
    Serial.println(F("========================================"));

    cameraReady = initCamera();
    wifiConnect();
    Serial.print(F("[BOOT] MAC ")); Serial.println(WiFi.macAddress());
    startHttp();
#ifdef ENABLE_ESPNOW_FALLBACK
    espnowCamInit();
#endif
    Serial.println(F("[BOOT] ready"));
}

void loop() {
    wifiMaintain();
#ifdef ENABLE_ESPNOW_FALLBACK
    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) { lastHeartbeat = millis(); espnowCamSendHeartbeat(); }
    processSerialInput();
#endif
    delay(10);
}
