#pragma once
#include <Arduino.h>

extern volatile bool buttonTriggered;
extern volatile bool endstopXTriggered;
extern volatile bool endstopYTriggered;
extern volatile bool endstopZTriggered;

void setupInterrupts();

void onButtonInterrupt();
void onEndstopXInterrupt();
void onEndstopYInterrupt();
void onEndstopZInterrupt();
