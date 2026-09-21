/**
 * robot_state.h — Central state manager for Robot Prototype
 */

#ifndef ROBOT_STATE_H
#define ROBOT_STATE_H

#include <cstdint>
#include <Arduino.h>
#include "command.h"

// ============================================================
// Central Robot State
// ============================================================
struct RobotState {
    CommandSource activeSource;
    CommandAction currentCommand;
    uint8_t       speed;

    float distanceCm;
    bool  obstacleDetected;

    bool wifiAlive;
    bool bluetoothAlive;
    bool espNowAlive;

    bool emergencyStop;
    bool fault;

    bool bumpFront;
    bool bumpRear;
    bool cliffFront;
    bool cliffRear;

    uint32_t uptime;
};

// ============================================================
// State Manager API
// ============================================================
void  stateInit();
RobotState& stateGet();

// Convenience setters
void stateSetSource(CommandSource src);
void stateSetCommand(CommandAction cmd);
void stateSetSpeed(uint8_t spd);
void stateSetDistance(float cm);
void stateSetObstacle(bool detected);
void stateSetWifiAlive(bool alive);
void stateSetBluetoothAlive(bool alive);
void stateSetEspNowAlive(bool alive);
void stateSetEmergencyStop(bool estop);
void stateSetFault(bool f);
void stateSetBump(bool front, bool rear);
void stateSetCliff(bool front, bool rear);
void stateUpdateUptime();

// JSON serialization for telemetry
String stateToJson();

#endif // ROBOT_STATE_H
