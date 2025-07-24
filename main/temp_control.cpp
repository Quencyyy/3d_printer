#include "temp_control.h"
#include "pins.h"
#include <Arduino.h>
#include <math.h>
#include <avr/wdt.h>
#include "state.h"
#include "tunes.h"

// Uncomment to enable verbose serial logging from readTemperature()
//#define DEBUG_LOGS

// Simple 100k thermistor using B=3950 equation
// Returns temperature in Celsius
float readThermistor(int pin) {
#if defined(SIMULATE_HEATER) || defined(SIMULATE_GCODE_INPUT)
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
extern const int buzzerPin;

void beepErrorAlert() {
    for (int i = 0; i < 5; i++) {
        tone(buzzerPin, 1000, 150);
        delay(200);
        wdt_reset();
    }
    noTone(buzzerPin);
}

void readTemperature() {
    float tempC = readThermistor(tempPin);
    static float filtered = 0.0f;
    static bool filterInit = false;
    if (!filterInit) {
        filtered = tempC;
        filterInit = true;
    } else {
        filtered = filtered * 0.7f + tempC * 0.3f;
    }
    printer.currentTemp = filtered;

    #ifdef DEBUG_LOGS
    static unsigned long lastLog = 0;
    unsigned long now = millis();
    if (now - lastLog >= 1000) {
        float voltage = printer.rawTemp * 5.0f / 1023.0f;

        float error = printer.setTemp - printer.currentTemp;
        float pwm = printer.pwmValue; // 請確認在 controlHeater 裡有設這值

        Serial.print(now);
        Serial.print(", ");
        Serial.print(printer.currentTemp);
        Serial.print(", ");
        Serial.print(printer.setTemp);
        Serial.print(", ");
        Serial.print((int)pwm);
        Serial.print(", ");
        Serial.print(printer.heaterOn ? "ON" : "OFF");
        Serial.print(", ");
        Serial.print(printer.Kp);
        Serial.print(", ");
        Serial.print(printer.Ki);
        Serial.print(", ");
        Serial.print(printer.Kd);
        Serial.print(", ");
        Serial.print(error);
        Serial.print(", ");
        Serial.println(printer.lastOutput);  // 建議你增加這個變數儲存 output

        lastLog = now;
    }
    #endif

    if (printer.currentTemp < -10 || printer.currentTemp > 300) {
        printer.tempError = true;
        printer.tempErrorNotified = false;
        printer.setTemp = 0;
        #if !(defined(SIMULATE_HEATER) || defined(SIMULATE_GCODE_INPUT))
        analogWrite(heaterPin, 0);
        #endif
        printer.heaterOn = false;
    }

    if (printer.tempError && !printer.tempErrorNotified) {
        beepErrorAlert();
        printer.tempErrorNotified = true;
    }
}

void controlHeater() {
    static unsigned long heatStart = 0;

    if (printer.setTemp > 0.0f) {
        unsigned long now = millis();

        if (heatStart == 0) {
            heatStart = now;
        }

        static int overshootCount = 0;
        if (printer.currentTemp > printer.setTemp + 15.0f) {
            overshootCount++;
            if (overshootCount >= 3) {
#if !(defined(SIMULATE_HEATER) || defined(SIMULATE_GCODE_INPUT))
                analogWrite(heaterPin, 0);
#endif
                printer.setTemp = 0;
                printer.heaterOn = false;
                Serial.println(F("ERROR: Overshoot"));
                heatStart = 0;
                return;
            }
        } else {
            overshootCount = 0;
        }

        if (heatStart > 0 && now - heatStart > 180000) {
#if !(defined(SIMULATE_HEATER) || defined(SIMULATE_GCODE_INPUT))
            analogWrite(heaterPin, 0);
#endif
            printer.setTemp = 0;
            printer.heaterOn = false;
            heatStart = 0;
            Serial.println(F("ERROR: Heat timeout"));
            return;
        }

        float elapsed = (now - printer.lastTime) / 1000.0f;
        elapsed = max(elapsed, 0.001f);
        printer.lastTime = now;

        float error = printer.setTemp - printer.currentTemp;
        printer.integral += error * elapsed;
        float derivative = (error - printer.previousError) / elapsed;
        printer.previousError = error;

        // PID output：範圍 0.0~1.0
        float rawOutput = printer.Kp * error + printer.Ki * printer.integral + printer.Kd * derivative;
        rawOutput = max(rawOutput, 0.0f);  // 不讓 PID 為負數

        static float lastTemp = 0.0f;
        float rampRate = (printer.currentTemp - lastTemp) / elapsed;
        lastTemp = printer.currentTemp;

        float deltaT = printer.setTemp - printer.currentTemp;
        int maxOut = 255;

        if (deltaT > 20.0f) {
            maxOut = 255;  // 全力加熱
        } else if (deltaT > 10.0f) {
            maxOut = 200;
        } else if (deltaT > 3.0f) {
            maxOut = (rampRate > 1.0f) ? 80 : 120;
        } else {
            maxOut = 80;
        }

        // 限制輸出比例不超過1，再乘 maxOut
        float outputRatio = constrain(rawOutput, 0.0f, 1.0f);
        float scaledOutput = outputRatio * maxOut;

        printer.lastOutput = scaledOutput;  // 儲存實際PWM輸出
        printer.pwmValue = scaledOutput;

#if !(defined(SIMULATE_HEATER) || defined(SIMULATE_GCODE_INPUT))
        analogWrite(heaterPin, (int)scaledOutput);
#endif
        printer.heaterOn = scaledOutput > 0;

        // 穩定判斷 + 音效提示
        if (abs(printer.currentTemp - printer.setTemp) < 1.0f) {
            if (!printer.heatDoneBeeped && heatStableStart == 0) {
                heatStableStart = now;
            }
            if (!printer.heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
#ifndef NO_TUNES
                playTune(TUNE_HEAT_DONE);
#endif
                printer.heatDoneBeeped = true;
            }
        } else {
            heatStableStart = 0;
        }
    } else {
#if !(defined(SIMULATE_HEATER) || defined(SIMULATE_GCODE_INPUT))
        analogWrite(heaterPin, 0);
#endif
        printer.heaterOn = false;
        printer.heatDoneBeeped = false;
        heatStableStart = 0;
        heatStart = 0;
    }
}


