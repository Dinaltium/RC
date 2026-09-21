/**
 * cliff.h — Drop-off detection with downward VL53L0X sensors
 *
 * A cliff is LATCHED: once seen, movement toward it stays blocked until
 * CMD_CLEAR_EMERGENCY. Hysteresis makes no sense at an edge.
 */

#ifndef CLIFF_H
#define CLIFF_H

void cliffInit();          // call after tofInit()
void cliffUpdate();        // call every loop (after tofUpdate)
bool cliffFront();         // latched: a drop ahead of the front wheels
bool cliffRear();          // latched: a drop behind the rear
void cliffClear();         // release both latches (only from CLEAR_EMERGENCY)
bool cliffSensorsOk();     // both configured sensors initialised

#endif // CLIFF_H
