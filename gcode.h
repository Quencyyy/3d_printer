// gcode.h
#ifndef GCODE_H
#define GCODE_H

#include <Arduino.h>

void processGcode();
void handleG1Axis(char axis, int stepPin, int dirPin, long& pos, String& gcode);
void homeAxis(int, int, int, const char*);

#endif