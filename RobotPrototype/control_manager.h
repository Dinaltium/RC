/**
 * control_manager.h — Priority-based command arbitration and failover
 */

#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include "command.h"

void controlInit();

// Submit a command from any source — arbitration happens here
void controlSubmitCommand(const RobotCommand& cmd);

// Update heartbeat tracking and failover logic — call every loop
void controlUpdate();

// Get current active source
CommandSource controlGetActiveSource();

// Record heartbeat from a source (without a movement command)
void controlHeartbeat(CommandSource source);

#endif // CONTROL_MANAGER_H
