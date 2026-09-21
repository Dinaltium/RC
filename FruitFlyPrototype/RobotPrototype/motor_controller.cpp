/**
 * motor_controller.cpp — PWM motor control implementation
 *
 * Uses analogWrite() on ESP32 Arduino Core 3.x+.
 * All PWM/LEDC details are isolated here per §14.
 */

#include "motor_controller.h"
#include "config.h"
#include <Arduino.h>

// ============================================================
// Pin tables for each motor
// ============================================================
struct MotorPins {
    uint8_t in1;
    uint8_t in2;
    bool    invert;
};

// Pins marked PIN_UNUSED are never configured or written. Only FL and FR are
// physically connected in the 2-motor build; GPIO23 is used for DRV8833 EEP.
static constexpr uint8_t PIN_UNUSED = 0xFF;

static const MotorPins motorPins[MOTOR_COUNT] = {
    { FL_IN1,     FL_IN2,     FL_INVERT },  // Front Left
    { FR_IN1,     FR_IN2,     FR_INVERT },  // Front Right
    { PIN_UNUSED, PIN_UNUSED, RL_INVERT },  // Rear Left  — not wired
    { PIN_UNUSED, PIN_UNUSED, RR_INVERT }   // Rear Right — not wired
};

static bool motorWired(uint8_t i) {
    return motorPins[i].in1 != PIN_UNUSED && motorPins[i].in2 != PIN_UNUSED;
}

static bool _stopped = true;

// ============================================================
// Initialization
// ============================================================
void motorInit() {
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    digitalWrite(MOTOR_ENABLE_PIN, HIGH);

    for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
        if (!motorWired(i)) continue;
        pinMode(motorPins[i].in1, OUTPUT);
        pinMode(motorPins[i].in2, OUTPUT);
        digitalWrite(motorPins[i].in1, LOW);
        digitalWrite(motorPins[i].in2, LOW);
        analogWriteResolution(motorPins[i].in1, PWM_RESOLUTION_BITS);
        analogWriteResolution(motorPins[i].in2, PWM_RESOLUTION_BITS);
    }

    _stopped = true;
    Serial.println(F("[MOTOR] Initialized — 2 motors, EEP GPIO23 HIGH"));
}

// ============================================================
// Set individual motor speed
// speed: -255 to +255 (positive = forward, negative = reverse)
// ============================================================
void motorSet(Motor m, int16_t speed) {
    if (m >= MOTOR_COUNT || !motorWired(m)) return;

    const MotorPins& mp = motorPins[m];

    // Apply direction inversion
    if (mp.invert) speed = -speed;

    // Clamp to valid range
    if (speed > 255) speed = 255;
    if (speed < -255) speed = -255;

    if (speed > 0) {
        // Forward: IN1 = PWM, IN2 = LOW
        analogWrite(mp.in1, (uint8_t)speed);
        analogWrite(mp.in2, 0);
    } else if (speed < 0) {
        // Reverse: IN1 = LOW, IN2 = PWM
        analogWrite(mp.in1, 0);
        analogWrite(mp.in2, (uint8_t)(-speed));
    } else {
        // Stop: both LOW (coast)
        analogWrite(mp.in1, 0);
        analogWrite(mp.in2, 0);
    }
}

// ============================================================
// Execute high-level command on the two connected motors
// ============================================================
void motorExecuteCommand(CommandAction action, uint8_t speed) {
    int16_t spd = (int16_t)speed;

    switch (action) {
        case CMD_FORWARD:
            motorSet(MOTOR_FL, spd);
            motorSet(MOTOR_FR, spd);
            _stopped = false;
            break;

        case CMD_BACKWARD:
            motorSet(MOTOR_FL, -spd);
            motorSet(MOTOR_FR, -spd);
            _stopped = false;
            break;

        case CMD_LEFT: {
            // Left side reduced, right side full
            int16_t inner = spd - TURN_SPEED_REDUCTION;
            if (inner < 0) inner = 0;
            motorSet(MOTOR_FL, inner);
            motorSet(MOTOR_FR, spd);
            _stopped = false;
            break;
        }

        case CMD_RIGHT: {
            // Right side reduced, left side full
            int16_t inner = spd - TURN_SPEED_REDUCTION;
            if (inner < 0) inner = 0;
            motorSet(MOTOR_FL, spd);
            motorSet(MOTOR_FR, inner);
            _stopped = false;
            break;
        }

        case CMD_ROTATE_LEFT:
            // Left side reverse, right side forward
            motorSet(MOTOR_FL, -spd);
            motorSet(MOTOR_FR, spd);
            _stopped = false;
            break;

        case CMD_ROTATE_RIGHT:
            // Left side forward, right side reverse
            motorSet(MOTOR_FL, spd);
            motorSet(MOTOR_FR, -spd);
            _stopped = false;
            break;

        case CMD_STOP:
        default:
            motorEmergencyStop();
            break;
    }
}

// ============================================================
// Emergency stop — all outputs to zero immediately
// ============================================================
void motorEmergencyStop() {
    for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
        if (!motorWired(i)) continue;
        analogWrite(motorPins[i].in1, 0);
        analogWrite(motorPins[i].in2, 0);
    }
    _stopped = true;
}

bool motorsStopped() {
    return _stopped;
}
