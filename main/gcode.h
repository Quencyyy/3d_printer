// gcode.h
#ifndef GCODE_H
#define GCODE_H

#include <Arduino.h>

void processGcode();
void sendOk(const String &msg = "");
#include "motion.h"

// Steps per millimeter settings used for movement
extern float stepsPerMM_X;
extern float stepsPerMM_Y;
extern float stepsPerMM_Z;
extern float stepsPerMM_E;

// Positioning mode flag (true = absolute mode)
extern bool useAbsolute;


#endif
