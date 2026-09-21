/**
 * motor_controller.h — 4-wheel motor control via DRV8833 drivers
 */

#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <cstdint>
#include "command.h"

// Motor identifiers
enum Motor : uint8_t {
    MOTOR_FL = 0,  // Front Left
    MOTOR_FR = 1,  // Front Right
    MOTOR_RL = 2,  // Rear Left
    MOTOR_RR = 3,  // Rear Right
    MOTOR_COUNT = 4
};

void motorInit();

// Set individual motor: positive speed = forward, negative = reverse, 0 = stop
void motorSet(Motor m, int16_t speed);

// Execute a high-level command on all 4 motors
void motorExecuteCommand(CommandAction action, uint8_t speed);

// Emergency stop — all outputs to zero immediately
void motorEmergencyStop();

// Check if motors are currently stopped
bool motorsStopped();

#endif // MOTOR_CONTROLLER_H
