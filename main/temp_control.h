#pragma once


#define PID_SCALE 100
#define PID_FACTOR (PID_SCALE * PID_SCALE)

#define SERIES_RESISTOR     10000.0f
#define THERMISTOR_NOMINAL  100000.0f
#define BCOEFFICIENT        3950.0f
#define TEMPERATURE_NOMINAL 25.0f

float readThermistor(int pin);
void readTemperature();
void controlHeater();
void beepErrorAlert();
extern const unsigned long stableHoldTime;
