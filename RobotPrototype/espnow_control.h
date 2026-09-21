/**
 * espnow_control.h — ESP-NOW receive handler for main ESP32
 *
 * Incoming packets are queued in a lightweight ISR-safe ring buffer
 * and processed in espnowUpdate() during loop().
 */

#ifndef ESPNOW_CONTROL_H
#define ESPNOW_CONTROL_H

void espnowInit();
void espnowUpdate();  // Call from loop() to process queued packets

#endif // ESPNOW_CONTROL_H
