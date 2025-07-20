#pragma once
#include <Arduino.h>

extern volatile bool buttonTriggered;

void setupInterrupts();
void onButtonInterrupt();
