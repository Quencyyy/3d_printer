#pragma once
#include <Arduino.h>

void moveAxis(int stepPin, int dirPin, float& pos, float target, int feedrate, char axis);

bool homeAxis(int stepPin, int dirPin, int endstopPin, const char* label, unsigned long timeoutMs);

void moveAxes(float targetX, float targetY, float targetZ, float targetE, int feedrate);
