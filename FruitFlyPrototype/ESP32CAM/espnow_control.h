/**
 * espnow_control.h — ESP-NOW sender for ESP32-CAM
 */

#ifndef ESPNOW_CONTROL_H
#define ESPNOW_CONTROL_H

#include <cstdint>

// Packet structure — must match main ESP32
struct RobotPacket {
    uint32_t magic;
    uint8_t  version;
    uint8_t  source;
    uint8_t  command;
    uint8_t  speed;
    uint32_t sequence;
    uint32_t timestamp;
} __attribute__((packed));

enum CamCommand : uint8_t {
    CMD_STOP            = 0,
    CMD_FORWARD         = 1,
    CMD_BACKWARD        = 2,
    CMD_LEFT            = 3,
    CMD_RIGHT           = 4,
    CMD_ROTATE_LEFT     = 5,
    CMD_ROTATE_RIGHT    = 6,
    CMD_HEARTBEAT       = 7,
    CMD_EMERGENCY_STOP  = 8,
    CMD_CLEAR_EMERGENCY = 9
};

// Source value for ESP32-CAM
constexpr uint8_t SRC_ESP32_CAM_ESPNOW = 3;

void espnowCamInit();
bool espnowCamSendCommand(uint8_t command, uint8_t speed);
bool espnowCamSendHeartbeat();

#endif // ESPNOW_CONTROL_H
