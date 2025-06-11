// gcode.h
#ifndef GCODE_H
#define GCODE_H

#include <Arduino.h>

void processGcode();
void handleG1Axis(char axis, int stepPin, int dirPin, long& pos, String& gcode);
void homeAxis(int, int, int, const char*);
void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate, char axis);

// Steps per millimeter settings used for movement
extern float stepsPerMM_X;
extern float stepsPerMM_Y;
extern float stepsPerMM_Z;
extern float stepsPerMM_E;


#endif
