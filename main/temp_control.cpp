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
    float resistance = (5.0f - voltage) * 10000.0f / voltage;
    float tempK = 1.0f / (log(resistance / 10000.0f) / 3950.0f + 1.0f / 298.15f);
    return tempK - 273.15f;
}

// External state variables defined in main.ino
extern float setTemp;
extern float currentTemp;
extern bool fanStarted;
extern bool fanForced;
extern bool heatDoneBeeped;
extern bool tempError;
extern bool tempErrorNotified;
extern float Kp, Ki, Kd;
extern float integral, previousError;
extern unsigned long lastTime;
extern unsigned long heatStableStart;
extern const unsigned long stableHoldTime;
extern bool fanOn;
extern bool heaterOn;

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
    currentTemp = readThermistor(tempPin);

    if (currentTemp < -10 || currentTemp > 300) {
        tempError = true;
        tempErrorNotified = false;
        setTemp = 0;
        analogWrite(heaterPin, 0);
        heaterOn = false;
        digitalWrite(fanPin, LOW);
        fanOn = false;
    }

    if (tempError && !tempErrorNotified) {
#ifdef ENABLE_BUZZER
        beepErrorAlert();
#endif
        tempErrorNotified = true;
    }
}

void controlHeater() {
    if (setTemp > 0.0) {
        unsigned long now = millis();
        float elapsed = (now - lastTime) / 1000.0;
        lastTime = now;

        float error = setTemp - currentTemp;
        integral += error * elapsed;
        float derivative = (error - previousError) / elapsed;
        previousError = error;

        float output = Kp * error + Ki * integral + Kd * derivative;
        output = constrain(output, 0, 255);
        analogWrite(heaterPin, (int)output);
        heaterOn = output > 0;

        if (currentTemp >= 50 && !fanStarted && !fanForced) {
            digitalWrite(fanPin, HIGH);
            fanOn = true;
            fanStarted = true;
        }

        if (abs(currentTemp - setTemp) < 1.0) {
            if (!heatDoneBeeped && heatStableStart == 0) {
                heatStableStart = now;
            }
            if (!heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
#ifdef ENABLE_BUZZER
                tone(buzzerPin, 1000, 200);
#endif
                heatDoneBeeped = true;
            }
        } else {
            heatStableStart = 0;
        }
    } else {
        analogWrite(heaterPin, 0);
        heaterOn = false;
        if (!fanForced) {
            digitalWrite(fanPin, LOW);
            fanOn = false;
            fanStarted = false;
        }
        heatDoneBeeped = false;
        heatStableStart = 0;
    }
}

