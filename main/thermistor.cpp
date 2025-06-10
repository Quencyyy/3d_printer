#include "thermistor.h"
#include <Arduino.h>
#include <math.h>

// Simple 100k thermistor using B=3950 equation
// Returns temperature in Celsius
float readThermistor(int pin) {
    int raw = analogRead(pin);
    float voltage = raw * 5.0f / 1023.0f;
    if (voltage <= 0.0f) {
        return -1000.0f; // invalid reading
    }
    float resistance = (5.0f - voltage) * 10000.0f / voltage;
    float tempK = 1.0f / (log(resistance / 10000.0f) / 3950.0f + 1.0f / 298.15f);
    return tempK - 273.15f;
}
