/**
 * bump.h — Front/rear bump switches (3D-printer endstops, normally closed)
 */

#ifndef BUMP_H
#define BUMP_H

void bumpInit();
void bumpUpdate();     // call every loop; debounced
bool bumpFront();      // true while the front bumper is pressed (or its wire is broken)
bool bumpRear();

#endif // BUMP_H
