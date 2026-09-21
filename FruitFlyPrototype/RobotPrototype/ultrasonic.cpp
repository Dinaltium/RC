/**
 * ultrasonic.cpp — Non-blocking HC-SR04 implementation
 *
 * Echo timing is done in a CHANGE interrupt on the ECHO pin, so loop()
 * never blocks waiting for the pulse (pulseIn() could stall up to
 * ULTRASONIC_TIMEOUT_US every cycle). Measurements are triggered every
 * ULTRASONIC_INTERVAL_MS; a missing echo is treated as "far" (999 cm).
 */

#include "ultrasonic.h"
#include "config.h"
#include "tof.h"
#include <Arduino.h>

static float    _distanceCm   = 999.0f;
static uint32_t _lastMeasure  = 0;

// Written from ISR, read from loop()
static volatile uint32_t _echoStartUs = 0;
static volatile uint32_t _echoWidthUs = 0;
static volatile bool     _echoDone    = false;
static bool              _echoPending = false;

static void IRAM_ATTR onEchoChange() {
    uint32_t now = micros();
    if (digitalRead(ULTRASONIC_ECHO) == HIGH) {
        _echoStartUs = now;
    } else {
        _echoWidthUs = now - _echoStartUs;
        _echoDone    = true;
    }
}

void ultrasonicInit() {
#if defined(FRONT_RANGE_LD2420)
    pinMode(LD2420_PRESENCE_PIN, INPUT);
    _distanceCm = 999.0f;
    Serial.println(F("[RANGE] HLK-LD2420 presence mode on GPIO27"));
    return;
#elif defined(FRONT_RANGE_VL53L0X)
    _distanceCm = 999.0f;
    Serial.println(tofAvailable(TOF_FRONT) ? F("[RANGE] VL53L0X front ToF")
                                           : F("[RANGE] WARNING: front VL53L0X missing; reading 999 cm"));
    return;
#endif
    pinMode(ULTRASONIC_TRIG, OUTPUT);
    pinMode(ULTRASONIC_ECHO, INPUT);
    digitalWrite(ULTRASONIC_TRIG, LOW);
    attachInterrupt(digitalPinToInterrupt(ULTRASONIC_ECHO), onEchoChange, CHANGE);
    Serial.println(F("[ULTRASONIC] Initialized (interrupt echo timing)"));
}

void ultrasonicUpdate() {
    uint32_t now = millis();

#if defined(FRONT_RANGE_LD2420)
    // LD2420 presence output: HIGH = occupied/presence, LOW = clear.
    // Expose presence through the existing distance/safety interface:
    // 0 cm means blocked; 999 cm means no detected presence.
    (void)now;
    _distanceCm = digitalRead(LD2420_PRESENCE_PIN) == HIGH ? 0.0f : 999.0f;
    return;
#elif defined(FRONT_RANGE_VL53L0X)
    (void)now;
    if (!tofAvailable(TOF_FRONT)) { _distanceCm = 999.0f; return; }
    uint16_t mm = tofReadMm(TOF_FRONT);
    _distanceCm = (mm == 0xFFFF) ? 999.0f : mm / 10.0f;   // out of range = far
    return;
#endif

    // Harvest a completed echo from the previous trigger
    if (_echoPending && _echoDone) {
        noInterrupts();
        uint32_t width = _echoWidthUs;
        _echoDone = false;
        interrupts();
        _echoPending = false;
        if (width == 0 || width > ULTRASONIC_TIMEOUT_US) {
            _distanceCm = 999.0f;
        } else {
            // Speed of sound ≈ 343 m/s → 0.0343 cm/µs, halved for the round trip
            _distanceCm = (float)width * 0.01715f;
        }
    }

    if (now - _lastMeasure < ULTRASONIC_INTERVAL_MS) return;

    // Previous echo never returned within the interval → no object in range
    if (_echoPending) {
        _distanceCm  = 999.0f;
        _echoPending = false;
    }
    _lastMeasure = now;

    // 10 µs trigger pulse (the only blocking part, ~12 µs)
    _echoDone = false;
    digitalWrite(ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG, LOW);
    _echoPending = true;
}

float ultrasonicGetDistanceCm() {
    return _distanceCm;
}
