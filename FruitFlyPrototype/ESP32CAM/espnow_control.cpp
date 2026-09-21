/**
 * espnow_control.cpp — ESP-NOW sender for ESP32-CAM
 *
 * Sends RobotPacket to the main ESP32 for heartbeat and commands.
 * Uses unicast when MAC is known, broadcast otherwise.
 */

#include "espnow_control.h"
#include "config.h"
#ifdef ENABLE_ESPNOW_FALLBACK
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Arduino.h>

static uint32_t _sequence = 0;

// Send callback for delivery confirmation
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println(F("[ESPNOW] Send failed"));
    }
}

void espnowCamInit() {
    // The sketch has already joined the robot's AP in STA mode, which puts
    // the radio on the AP's channel; ESP-NOW rides on that same channel.

    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[ESPNOW] ERROR: Init failed"));
        return;
    }

    esp_now_register_send_cb(onDataSent);

    // Add main ESP32 as peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MAIN_ESP32_MAC, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println(F("[ESPNOW] WARNING: Failed to add main ESP32 peer"));
    }

    Serial.println(F("[ESPNOW] Initialized — ready to send"));
}

bool espnowCamSendCommand(uint8_t command, uint8_t speed) {
    RobotPacket pkt;
    pkt.magic     = ESPNOW_MAGIC;
    pkt.version   = ESPNOW_VERSION;
    pkt.source    = SRC_ESP32_CAM_ESPNOW;
    pkt.command   = command;
    pkt.speed     = speed;
    pkt.sequence  = _sequence++;
    pkt.timestamp = millis();

    esp_err_t result = esp_now_send(MAIN_ESP32_MAC, (uint8_t*)&pkt, sizeof(pkt));
    return (result == ESP_OK);
}

bool espnowCamSendHeartbeat() {
    return espnowCamSendCommand(CMD_HEARTBEAT, 0);
}
#endif // ENABLE_ESPNOW_FALLBACK
