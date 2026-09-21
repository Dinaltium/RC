/**
 * tof.h — Shared VL53L0X time-of-flight sensor management
 *
 * All VL53L0X modules share one I2C bus. They all boot at 0x29, so each
 * is held in reset via XSHUT, woken one at a time and re-addressed.
 * Readings are continuous (sensor-side) and polled without blocking.
 */

#ifndef TOF_H
#define TOF_H

#include <cstdint>

enum TofId : uint8_t {
    TOF_FRONT       = 0,  // forward range (only if FRONT_RANGE_VL53L0X)
    TOF_CLIFF_FRONT = 1,
    TOF_CLIFF_REAR  = 2,
    TOF_COUNT       = 3
};

void     tofInit();                    // brings up every sensor that is enabled in config.h
bool     tofAvailable(TofId id);       // sensor initialised OK
uint16_t tofReadMm(TofId id);          // latest reading; 0xFFFF = timeout / out of range
void     tofUpdate();                  // call from loop(): polls sensors at TOF_INTERVAL_MS

#endif // TOF_H
