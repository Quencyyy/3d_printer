#include "temp_control.h"
#include "pins.h"
#include <Arduino.h>
#include <math.h>
#include <avr/wdt.h>
#include "state.h"
#include "tunes.h"

// Simple 100k thermistor using B=3950 equation
// Returns temperature in Celsius
float readThermistor(int pin) {
#ifdef DEBUG_INPUT
    // In debug mode simulate a simple linear temperature ramp
    static float simTemp = 25.0f;
    if (printer.setTemp > simTemp) {
        simTemp += 1.0f;  // increase 1 degC per call
        if (simTemp > printer.setTemp) simTemp = printer.setTemp;
    } else if (printer.setTemp <= 0.0f && simTemp > 25.0f) {
        simTemp -= 1.0f;  // cool down when heater off
        if (simTemp < 25.0f) simTemp = 25.0f;
    }
    printer.rawTemp = (int)(simTemp * 2);  // dummy value for debugging
    return simTemp;
#else
    int raw = analogRead(pin);
    printer.rawTemp = raw; // keep raw reading for debugging
    float voltage = raw * 5.0f / 1023.0f;
    if (voltage <= 0.0f) {
        return -1000.0f; // invalid reading
    }
    float resistance = (5.0f - voltage) * SERIES_RESISTOR / voltage;
    float tempK = 1.0f / (log(resistance / THERMISTOR_NOMINAL) / BCOEFFICIENT +
                         1.0f / (TEMPERATURE_NOMINAL + 273.15f));
    return tempK - 273.15f;
#endif
}

// External state variables defined in main.ino
extern unsigned long heatStableStart;
extern const unsigned long stableHoldTime;


// Pins from pins.h
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
        wdt_reset();
    }
    noTone(buzzerPin);
#endif
}

void readTemperature() {
    printer.currentTemp = readThermistor(tempPin);

    static unsigned long lastLog = 0;
    unsigned long now = millis();
    if (now - lastLog >= 1000) {
        float voltage = printer.rawTemp * 5.0f / 1023.0f;
        /* log
        Serial.print(F("Thermistor ADC:"));
        Serial.print(printer.rawTemp);
        Serial.print(F(" V:"));
        Serial.print(voltage, 3);
        Serial.print(F(" T:"));
        Serial.println(printer.currentTemp);
        log */
        lastLog = now;
    }

    if (printer.currentTemp < -10 || printer.currentTemp > 300) {
        printer.tempError = true;
        printer.tempErrorNotified = false;
        printer.setTemp = 0;
        #ifndef DEBUG_INPUT
        analogWrite(heaterPin, 0);
        #endif
        printer.heaterOn = false;
    }

    if (printer.tempError && !printer.tempErrorNotified) {
#ifdef ENABLE_BUZZER
        beepErrorAlert();
#endif
        printer.tempErrorNotified = true;
    }
}

void controlHeater() {
    static unsigned long heatStart = 0;

    if (printer.setTemp > 0.0) {
        unsigned long now = millis();

        if (heatStart == 0) {
            heatStart = now;
        }

        if (printer.currentTemp > printer.setTemp + 15) {
#ifndef DEBUG_INPUT
            analogWrite(heaterPin, 0);
#endif
            printer.setTemp = 0;
            printer.heaterOn = false;
            Serial.println("!! ERROR: Overshoot too high, heater disabled.");
            heatStart = 0;
            return;
        }

        if (heatStart > 0 && now - heatStart > 180000) {
#ifndef DEBUG_INPUT
            analogWrite(heaterPin, 0);
#endif
            printer.setTemp = 0;
            printer.heaterOn = false;
            heatStart = 0;
            Serial.println("!! ERROR: Heating timeout, heater disabled.");
            return;
        }

        float elapsed = (now - printer.lastTime) / 1000.0f;
        elapsed = max(elapsed, 0.001f); // avoid divide by zero
        printer.lastTime = now;

        float error = printer.setTemp - printer.currentTemp;
        printer.integral += error * elapsed;
        float derivative = (error - printer.previousError) / elapsed;
        printer.previousError = error;

        float output = printer.Kp * error + printer.Ki * printer.integral + printer.Kd * derivative;

        // 模擬預熱區限速，避免加熱過快造成大幅超溫
        int maxOut = 255;
        if (printer.currentTemp < printer.setTemp - 20) {
            // 全力加熱
            maxOut = 255;
        } else if (printer.currentTemp < printer.setTemp - 10) {
            // 中速區
            maxOut = 150;
        } else {
            // 靠近目標，慢慢來
            maxOut = 80;
        }
        output = constrain(output, 0, maxOut);
        #ifndef DEBUG_INPUT
        analogWrite(heaterPin, (int)output);
        #endif
        printer.heaterOn = output > 0;

        if (abs(printer.currentTemp - printer.setTemp) < 1.0) {
            if (!printer.heatDoneBeeped && heatStableStart == 0) {
                heatStableStart = now;
            }
            if (!printer.heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
#ifdef ENABLE_BUZZER
                playTune(TUNE_HEAT_DONE);
#endif
                printer.heatDoneBeeped = true;
            }
        } else {
            heatStableStart = 0;
        }
    } else {
        #ifndef DEBUG_INPUT
        analogWrite(heaterPin, 0);
        #endif
        printer.heaterOn = false;
        printer.heatDoneBeeped = false;
        heatStableStart = 0;
        heatStart = 0; // reset
    }
}

