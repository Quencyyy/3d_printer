#pragma once

void initButton(int pin, unsigned long debounceMs = 25);
void updateButton();
bool isPressed();
bool justPressed();
bool longPressed(unsigned long ms);
