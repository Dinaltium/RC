/**
 * command_parser.h — Parse commands from all sources into RobotCommand
 */

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "command.h"
#include <Arduino.h>

// Parse a single serial character into a command (for serial debug and ESP32-CAM test)
bool parseSerialChar(char c, CommandSource source, uint8_t speed, RobotCommand& out);

// Parse a JSON command body from HTTP POST
bool parseJsonCommand(const String& body, RobotCommand& out);

#endif // COMMAND_PARSER_H
