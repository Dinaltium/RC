/**
 * ultrasonic.h — Non-blocking HC-SR04 ultrasonic distance sensor
 */

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

void  ultrasonicInit();
void  ultrasonicUpdate();   // call every loop iteration
float ultrasonicGetDistanceCm();

#endif // ULTRASONIC_H
