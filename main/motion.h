#pragma once
#include <Arduino.h>

void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate, char axis);

void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label);
