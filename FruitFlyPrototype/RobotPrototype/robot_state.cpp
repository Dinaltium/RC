/**
 * robot_state.cpp — Central state implementation
 */

#include "robot_state.h"
#include "config.h"
#include <Arduino.h>

static RobotState _state;

void stateInit() {
    _state.activeSource    = SRC_NONE;
    _state.currentCommand  = CMD_STOP;
    _state.speed           = DEFAULT_SPEED;
    _state.distanceCm      = 999.0f;
    _state.obstacleDetected = false;
    _state.wifiAlive       = false;
    _state.bluetoothAlive  = false;
    _state.espNowAlive     = false;
    _state.emergencyStop   = false;
    _state.fault           = false;
    _state.bumpFront = _state.bumpRear = false;
    _state.cliffFront = _state.cliffRear = false;
    _state.uptime          = 0;
}

RobotState& stateGet() {
    return _state;
}

void stateSetSource(CommandSource src)    { _state.activeSource = src; }
void stateSetCommand(CommandAction cmd)   { _state.currentCommand = cmd; }
void stateSetSpeed(uint8_t spd)           { _state.speed = spd; }
void stateSetDistance(float cm)           { _state.distanceCm = cm; }
void stateSetObstacle(bool detected)     { _state.obstacleDetected = detected; }
void stateSetWifiAlive(bool alive)       { _state.wifiAlive = alive; }
void stateSetBluetoothAlive(bool alive)  { _state.bluetoothAlive = alive; }
void stateSetEspNowAlive(bool alive)     { _state.espNowAlive = alive; }
void stateSetEmergencyStop(bool estop)   { _state.emergencyStop = estop; }
void stateSetFault(bool f)               { _state.fault = f; }
void stateSetBump(bool front, bool rear)  { _state.bumpFront = front; _state.bumpRear = rear; }
void stateSetCliff(bool front, bool rear) { _state.cliffFront = front; _state.cliffRear = rear; }

void stateUpdateUptime() {
    _state.uptime = millis();
}

String stateToJson() {
    String json = "{";
    json += "\"source\":\"";      json += sourceName(_state.activeSource);   json += "\",";
    json += "\"command\":\"";     json += actionName(_state.currentCommand); json += "\",";
    json += "\"speed\":";         json += String(_state.speed);              json += ",";
    json += "\"distance_cm\":";   json += String(_state.distanceCm, 1);     json += ",";
    json += "\"obstacle\":";      json += _state.obstacleDetected ? "true" : "false"; json += ",";
    json += "\"wifi_connected\":";      json += _state.wifiAlive ? "true" : "false";       json += ",";
    json += "\"bluetooth_connected\":"; json += _state.bluetoothAlive ? "true" : "false";  json += ",";
    json += "\"espnow_connected\":";    json += _state.espNowAlive ? "true" : "false";     json += ",";
    json += "\"emergency_stop\":"; json += _state.emergencyStop ? "true" : "false"; json += ",";
    json += "\"bump_front\":";    json += _state.bumpFront ? "true" : "false";  json += ",";
    json += "\"bump_rear\":";     json += _state.bumpRear ? "true" : "false";   json += ",";
    json += "\"cliff_front\":";   json += _state.cliffFront ? "true" : "false"; json += ",";
    json += "\"cliff_rear\":";    json += _state.cliffRear ? "true" : "false";  json += ",";
    json += "\"uptime_ms\":";     json += String(_state.uptime);             json += ",";
    json += "\"fault\":";         json += _state.fault ? "true" : "false";
    json += "}";
    return json;
}
