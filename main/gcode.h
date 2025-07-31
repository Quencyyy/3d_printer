// gcode.h
#ifndef GCODE_H
#define GCODE_H

#include <Arduino.h>

void processGcode();
void sendOk(const __FlashStringHelper* msg = nullptr);
void sendOk(const char* msg);
void sendOk();
void enterPauseMode();
#include "motion.h"

// Steps per millimeter settings used for movement
extern float stepsPerMM_X;
extern float stepsPerMM_Y;
extern float stepsPerMM_Z;
extern float stepsPerMM_E;

// Positioning mode flag for X/Y/Z axes (true = absolute mode)
extern bool useAbsoluteXYZ;
extern bool useRelativeE;
extern float feedrateMultiplier;
extern float flowrateMultiplier;


#endif
