/**
 * wifi_control.cpp — Wi-Fi Access Point initialization
 *
 * ESP32 operates as a soft AP so the phone can connect directly.
 * Fixed channel is used so ESP-NOW can coexist reliably.
 */

#include "wifi_control.h"
#include "config.h"
#include <WiFi.h>
#include <Arduino.h>

void wifiInit() {
    // Set Wi-Fi mode to AP + STA (required for ESP-NOW coexistence)
    WiFi.mode(WIFI_AP_STA);

    // Start soft AP on fixed channel
    bool ok = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

    if (ok) {
        Serial.println(F("[WIFI] Access Point started"));
        Serial.print(F("[WIFI] SSID: "));
        Serial.println(WIFI_SSID);
        Serial.print(F("[WIFI] IP:   "));
        Serial.println(WiFi.softAPIP());
        Serial.print(F("[WIFI] Channel: "));
        Serial.println(WIFI_CHANNEL);
    } else {
        Serial.println(F("[WIFI] ERROR: Failed to start Access Point"));
    }

    // Print MAC address for ESP-NOW peer registration
    Serial.print(F("[WIFI] MAC: "));
    Serial.println(WiFi.macAddress());
}
