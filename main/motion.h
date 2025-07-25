#pragma once
#include <Arduino.h>

void moveAxis(int stepPin, int dirPin, float& pos, float target, int feedrate, char axis);

void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label);

void moveAxes(float targetX, float targetY, float targetZ, float targetE, int feedrate);
