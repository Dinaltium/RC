/**
 * oled.h — Optional SSD1306 status display (ENABLE_OLED in config.h)
 *
 * Shows what the robot is doing, what it sees and who is driving it, so
 * the chassis can be debugged without a serial cable or the dashboard.
 */
#ifndef OLED_H
#define OLED_H

void oledInit();     // probes OLED_ADDR; silently disables itself if nothing answers
void oledUpdate();   // call from loop(); redraws every OLED_INTERVAL_MS
bool oledPresent();

#endif // OLED_H
