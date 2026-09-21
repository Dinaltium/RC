/**
 * bluetooth_control.cpp — Bluetooth Serial (SPP) fallback
 *
 * Uses BluetoothSerial from the ESP32 Arduino core.
 * Same RobotCommand format, same heartbeat/timeout logic.
 * Feeds commands into the same Control Manager.
 *
 * Guarded by ENABLE_BLUETOOTH — simultaneous Wi-Fi + ESP-NOW + BT
 * may be unreliable on some ESP32 configurations.
 */

#include "bluetooth_control.h"
#include "config.h"
#include "command.h"
#include "command_parser.h"
#include "control_manager.h"
#include "robot_state.h"
#include <Arduino.h>

#ifdef ENABLE_BLUETOOTH
#include <BluetoothSerial.h>

static BluetoothSerial SerialBT;

void bluetoothInit() {
    if (SerialBT.begin(BT_DEVICE_NAME)) {
        Serial.print(F("[BT] Bluetooth Serial started: "));
        Serial.println(BT_DEVICE_NAME);
    } else {
        Serial.println(F("[BT] ERROR: Bluetooth init failed"));
    }
}

void bluetoothUpdate() {
    while (SerialBT.available()) {
        char c = SerialBT.read();
        RobotCommand cmd;
        uint8_t speed = stateGet().speed;

        if (parseSerialChar(c, SRC_PHONE_BLUETOOTH, speed, cmd)) {
            controlSubmitCommand(cmd);
        } else if (c == 'h' || c == 'H') {
            // Heartbeat character
            controlHeartbeat(SRC_PHONE_BLUETOOTH);
        }
    }
}

#else
// Bluetooth disabled — stub implementations
void bluetoothInit() {
    Serial.println(F("[BT] Bluetooth disabled (ENABLE_BLUETOOTH not defined)"));
}

void bluetoothUpdate() {
    // no-op
}
#endif // ENABLE_BLUETOOTH
