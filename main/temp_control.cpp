#include "temp_control.h"
#include "pins.h"
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
    float resistance = (5.0f - voltage) * SERIES_RESISTOR / voltage;
    float tempK = 1.0f / (log(resistance / THERMISTOR_NOMINAL) / BCOEFFICIENT +
                         1.0f / (TEMPERATURE_NOMINAL + 273.15f));
    return tempK - 273.15f;
}

// External state variables defined in main.ino
#include "state.h"
extern unsigned long heatStableStart;
extern const unsigned long stableHoldTime;


// Pins from pins.h
extern const int fanPin;
extern const int heaterPin;
extern const int tempPin;
#ifdef ENABLE_BUZZER
extern const int buzzerPin;
#endif

void beepErrorAlert() {
#ifdef ENABLE_BUZZER
    for (int i = 0; i < 5; i++) {
        tone(buzzerPin, 1000, 150);
        delay(200);
    }
    noTone(buzzerPin);
#endif
}

void readTemperature() {
    printer.currentTemp = readThermistor(tempPin);

    if (printer.currentTemp < -10 || printer.currentTemp > 300) {
        printer.tempError = true;
        printer.tempErrorNotified = false;
        printer.setTemp = 0;
        analogWrite(heaterPin, 0);
        printer.heaterOn = false;
        digitalWrite(fanPin, LOW);
        printer.fanOn = false;
    }

    if (printer.tempError && !printer.tempErrorNotified) {
#ifdef ENABLE_BUZZER
        beepErrorAlert();
#endif
        printer.tempErrorNotified = true;
    }
}

void controlHeater() {
    if (printer.setTemp > 0.0) {
        unsigned long now = millis();
        float elapsed = (now - printer.lastTime) / 1000.0;
        printer.lastTime = now;

        float error = printer.setTemp - printer.currentTemp;
        printer.integral += error * elapsed;
        float derivative = (error - printer.previousError) / elapsed;
        printer.previousError = error;

        float output = printer.Kp * error + printer.Ki * printer.integral + printer.Kd * derivative;
        output = constrain(output, 0, 255);
        analogWrite(heaterPin, (int)output);
        printer.heaterOn = output > 0;

        if (printer.currentTemp >= 50 && !printer.fanStarted && !printer.fanForced) {
            digitalWrite(fanPin, HIGH);
            printer.fanOn = true;
            printer.fanStarted = true;
        }

        if (abs(printer.currentTemp - printer.setTemp) < 1.0) {
            if (!printer.heatDoneBeeped && heatStableStart == 0) {
                heatStableStart = now;
            }
            if (!printer.heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
#ifdef ENABLE_BUZZER
                tone(buzzerPin, 1000, 200);
#endif
                printer.heatDoneBeeped = true;
            }
        } else {
            heatStableStart = 0;
        }
    } else {
        analogWrite(heaterPin, 0);
        printer.heaterOn = false;
        if (!printer.fanForced) {
            digitalWrite(fanPin, LOW);
            printer.fanOn = false;
            printer.fanStarted = false;
        }
        printer.heatDoneBeeped = false;
        heatStableStart = 0;
    }
}

