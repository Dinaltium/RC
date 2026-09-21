/**
 * bluetooth_control.h — Bluetooth Serial fallback
 *
 * Compile-guarded: only active when ENABLE_BLUETOOTH is defined in config.h
 */

#ifndef BLUETOOTH_CONTROL_H
#define BLUETOOTH_CONTROL_H

void bluetoothInit();
void bluetoothUpdate();  // call every loop iteration

#endif // BLUETOOTH_CONTROL_H
