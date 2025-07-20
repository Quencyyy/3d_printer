#define ENABLE_HOMING
// Optional test modes (disabled by default)
//#define ENABLE_BUTTON_MENU_TEST
//#define ENABLE_AXIS_CYCLE_TEST
// Uncomment to feed predefined G-code without host software
//#define DEBUG_INPUT

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
// #include "button.h"
#include <EEPROM.h>
// #include "pins.h"
// #include "temp_control.h"
// #include "gcode.h"
// #include "tunes.h"
// #include "test_modes.h"
// #include "state.h"
// #include "motion.h"
// #include "interrupts.h"

/* ===== Begin inlined header files ===== */

// --- button.h ---
#pragma once

void initButton(int pin, unsigned long debounceMs = 25);
void updateButton();
bool isPressed();
bool justPressed();
bool longPressed(unsigned long ms);
bool doublePressed(unsigned long ms);

// --- pins.h ---
#pragma once

// 馬達控制腳位
extern const int stepPinX, dirPinX;
extern const int stepPinY, dirPinY;
extern const int stepPinZ, dirPinZ;
extern const int stepPinE, dirPinE;

// 硬體控制腳位
extern const int fanPin;
extern const int heaterPin;
extern const int tempPin;

// Buzzer pin defaults to D8, override with -DBUZZER_PIN=<pin>
extern const int buzzerPin;
extern const int motorEnablePin;
extern const int buttonPin;
extern const int endstopX;
extern const int endstopY;
extern const int endstopZ;

// 軟體旗標與限制
extern int eMaxSteps;

// --- temp_control.h ---
#pragma once

#define SERIES_RESISTOR     10000.0f
#define THERMISTOR_NOMINAL  100000.0f
#define BCOEFFICIENT        3950.0f
#define TEMPERATURE_NOMINAL 25.0f

float readThermistor(int pin);
void readTemperature();
void controlHeater();
void beepErrorAlert();
extern const unsigned long stableHoldTime;

// --- gcode.h ---
#ifndef GCODE_H
#define GCODE_H

#include <Arduino.h>

void processGcode();
void handleG1Axis(char axis, int stepPin, int dirPin, long& pos, String& gcode);
void sendOk(const String &msg = "");
#include "motion.h"

extern float stepsPerMM_X;
extern float stepsPerMM_Y;
extern float stepsPerMM_Z;
extern float stepsPerMM_E;

extern bool useAbsolute;

#endif

// --- tunes.h ---
#pragma once
#include <Arduino.h>

// Uncomment to enable buzzer features
// #define ENABLE_BUZZER

enum TuneType {
    TUNE_MARIO = 0,
    TUNE_CANON,
    TUNE_STAR_WARS,
    TUNE_TETRIS,
    TUNE_COUNT
};

#ifdef ENABLE_BUZZER
void playTune(int tune);
#else
inline void playTune(int tune) {}
#endif

// --- test_modes.h ---
#pragma once

// Define one of these macros to enable corresponding test mode
// #define ENABLE_BUTTON_MENU_TEST
// #define ENABLE_AXIS_CYCLE_TEST

#ifdef ENABLE_BUTTON_MENU_TEST
void testMenuSetup();
void testMenuLoop();
#endif

#ifdef ENABLE_AXIS_CYCLE_TEST
void axisTestSetup();
void axisTestLoop();
#endif

// --- state.h ---
#pragma once

struct PrinterState {
    float setTemp;
    float currentTemp;
    int rawTemp;
    bool tempError;
    bool tempErrorNotified;
    bool heatDoneBeeped;

    long posX, posY, posZ, posE;
    long eStart, eTotal;
    int progress;
    bool eStartSynced;

    bool fanOn;
    bool fanStarted;
    bool fanForced;
    bool heaterOn;

    char movingAxis;
    int movingDir;
    unsigned long lastMoveTime;

    float Kp, Ki, Kd;
    float integral, previousError;
    unsigned long lastTime;

    int currentTune;
};

extern PrinterState printer;

void resetPrinterState();
void updateProgress();

// --- motion.h ---
#pragma once
#include <Arduino.h>

void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate, char axis);

#ifdef ENABLE_HOMING
void homeAxis(int stepPin, int dirPin, volatile bool &triggered, const char* label);
#endif

// --- interrupts.h ---
#pragma once
#include <Arduino.h>

extern volatile bool buttonTriggered;

void setupInterrupts();
void onButtonInterrupt();

/* ===== End inlined header files ===== */

LiquidCrystal_I2C lcd(0x27, 16, 2);

int currentFeedrate = 1000;  // 預設速度 mm/min
unsigned long heatStableStart = 0;
const unsigned long stableHoldTime = 3000;

bool useAbsolute = true;

float stepsPerMM_X = 80.0;
float stepsPerMM_Y = 80.0;
float stepsPerMM_Z = 80.0;
float stepsPerMM_E = 80.0;

int displayMode = 0;
unsigned long lastPressTime = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long autoSwitchDelay = 30000;
const unsigned long idleSwitchDelay = 30000;
bool isLongPress = false;
bool confirmStop = false;
unsigned long confirmStartTime = 0;
bool displayFrozen = false;
unsigned long freezeStartTime = 0;
const unsigned long freezeDuration = 3000;

unsigned long lastLoopTime = 0;
const unsigned long loopInterval = 100;

char lastDisplayContent[33] = {0};

// 進度估算變數由 state 模組管理

void saveSettingsToEEPROM() {
    EEPROM.put(0, printer.Kp);
    EEPROM.put(4, printer.Ki);
    EEPROM.put(8, printer.Kd);
    EEPROM.put(12, printer.setTemp);
    EEPROM.put(16, stepsPerMM_X);
    EEPROM.put(20, stepsPerMM_Y);
    EEPROM.put(24, stepsPerMM_Z);
    EEPROM.put(28, stepsPerMM_E);
}

void loadSettingsFromEEPROM() {
    EEPROM.get(0, printer.Kp);
    EEPROM.get(4, printer.Ki);
    EEPROM.get(8, printer.Kd);
    EEPROM.get(12, printer.setTemp);
    EEPROM.get(16, stepsPerMM_X);
    EEPROM.get(20, stepsPerMM_Y);
    EEPROM.get(24, stepsPerMM_Z);
    EEPROM.get(28, stepsPerMM_E);
}




void clearTempError() {
    if (printer.currentTemp > -10 && printer.currentTemp < 300) {
        printer.tempError = false;
        printer.tempErrorNotified = false;
        showMessage("Sensor OK", "System Normal");
        delay(500);
        memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
    }
}


void showMessage(const char* line1, const char* line2) {
    char newContent[33];
    for (int i = 0; i < 16; i++) {
        newContent[i] = (i < strlen(line1)) ? line1[i] : ' ';
        newContent[i + 16] = (i < strlen(line2)) ? line2[i] : ' ';
    }
    newContent[32] = '\0';

    if (memcmp(newContent, lastDisplayContent, 32) == 0) return;

    for (int i = 0; i < 32; i++) {
        if (newContent[i] != lastDisplayContent[i]) {
            lcd.setCursor(i % 16, i / 16);
            lcd.print(newContent[i]);
            lastDisplayContent[i] = newContent[i];
        }
    }
}

void displayTempScreen() {
    char cur[8];
    char set[8];
    dtostrf(printer.currentTemp, 4, 1, cur);
    dtostrf(printer.setTemp, 3, 0, set);
    char buf[17];
    snprintf(buf, sizeof(buf), "T:%s%cC Set:%s", cur, 223, set);
    showMessage(buf, "");
}

void displayCoordScreen() {
    char buf1[17], buf2[17];
    snprintf(buf1, sizeof(buf1), "X%ld Y%ld", printer.posX, printer.posY);
    snprintf(buf2, sizeof(buf2), "Z%ld E%ld", printer.posZ, printer.posE);
    showMessage(buf1, buf2);
}

void displayStatusScreen() {
    if (printer.tempError) {
        showMessage("Sensor ERROR!", "Check & Press Btn");
        return;
    }

    if (printer.eTotal == 0) {
        showMessage("Print Complete", "Press Button");
        return;
    }

    long printed = printer.posE - printer.eStart;

    if (printer.eTotal < 0) {
        showMessage("No Print Job", "");
        return;
    }

    char bar[11];
    int filled = constrain(printer.progress / 10, 0, 10);
    for (int i = 0; i < 10; i++) {
        bar[i] = (i < filled) ? '#' : '-';
    }
    bar[10] = '\0';

    char line1[17];
    if (printer.progress >= 100)
        snprintf(line1, sizeof(line1), "[%s]%3d%%", bar, printer.progress);
    else
        snprintf(line1, sizeof(line1), "[%s] %3d%%", bar, printer.progress);
    char line2[17];
    snprintf(line2, sizeof(line2), "%ld/%ld", printed, printer.eTotal);
    showMessage(line1, line2);
}

void displayIdleScreen(int animPos) {
    static const char msg[] = "Waiting job";
    static unsigned long lastScroll = 0;
    static int offset = 0;
    const unsigned long scrollInterval = 400;

    int len = strlen(msg);
    char line1[17];
    if (len <= 15) {
        snprintf(line1, sizeof(line1), "%-15s", msg);
    } else {
        if (millis() - lastScroll > scrollInterval) {
            offset = (offset + 1) % (len + 1);
            lastScroll = millis();
        }
        for (int i = 0; i < 15; i++) {
            int idx = (offset + i) % (len + 1);
            line1[i] = (idx < len) ? msg[idx] : ' ';
        }
    }
    line1[15] = '\0';
    char line2[17];
    char tempBuf[8];
    if (printer.currentTemp < -10 || printer.currentTemp > 300) {
        snprintf(tempBuf, sizeof(tempBuf), "_%cC", 223);
    } else {
        snprintf(tempBuf, sizeof(tempBuf), "%d%cC", (int)round(printer.currentTemp), 223);
    }
    snprintf(line2, sizeof(line2), "%-14s>>", tempBuf);
    static const char anim[] = "|/-\\";
    line1[15] = anim[animPos];
    showMessage(line1, line2);
}

void updateLCD() {
    if (displayFrozen) {
        if (millis() - freezeStartTime < freezeDuration) {
            return;
        }
        displayFrozen = false;
        memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
    }
    static int animPos = 0;
    static const char anim[] = "|/-\\";

    bool idle = (printer.eTotal == -1 && millis() - lastPressTime >= idleSwitchDelay);
    if (idle) {
        displayIdleScreen(animPos);
    } else if (displayMode == 0) {
        displayTempScreen();
    } else if (displayMode == 1) {
        displayCoordScreen();
    } else {
        displayStatusScreen();
    }

    bool moving = (millis() - printer.lastMoveTime) < 1000 && printer.movingAxis != ' ';
    lcd.setCursor(12, 1);
    lcd.print(printer.fanOn ? 'F' : ' ');
    lcd.print(printer.heaterOn ? 'H' : ' ');
    if (moving) {
        lcd.print(printer.movingAxis);
        lcd.print(printer.movingDir > 0 ? '>' : '<');
    } else {
        lcd.print(' ');
        lcd.print(anim[animPos]);
        animPos = (animPos + 1) % 4;
    }
}


void checkButton() {
    updateButton();
    bool state = isPressed();
    static bool prevState = false;
    static unsigned long pressStartTime = 0;
    unsigned long now = millis();

    if (printer.eTotal == 0) {
        if (justPressed()) {
            printer.eTotal = -1;
            printer.progress = 0;
            printer.eStartSynced = false;
            memset(lastDisplayContent, 0, sizeof(lastDisplayContent)); // force LCD refresh
        }
        prevState = state;
        return;
    }

    if (printer.tempError) {
        clearTempError();
        prevState = state;
        return;
    }

    if (justPressed()) {
        pressStartTime = now;
        if (confirmStop) {
            confirmStartTime = now;
        }
    }

    if (state && !isLongPress && now - pressStartTime > 50) {
        if (longPressed(3000)) {
            if (confirmStop) {
                if (now - confirmStartTime >= 3000) {
                    forceStop();
                    confirmStop = false;
                }
            } else {
                confirmStop = true;
                confirmStartTime = now;
                showMessage("Confirm Stop?", "Hold 3s again");
            }
            isLongPress = true;
        }
    }

    if (!state && prevState) {
        if (confirmStop && now - confirmStartTime < 5000) {
            confirmStop = false;
            showMessage("Cancelled", "");
            delay(300);
        } else if (!isLongPress) {
            displayMode = (displayMode + 1) % 3;
            lastDisplaySwitch = now;
        }
        isLongPress = false;
        lastPressTime = now;
    }

    prevState = state;
}

void autoSwitchDisplay() {
    if (displayMode != 2 && printer.progress < 100 && printer.eStartSynced && printer.eTotal > 0) {
        unsigned long now = millis();
        if (now - lastPressTime >= autoSwitchDelay && now - lastDisplaySwitch >= autoSwitchDelay) {
            displayMode = 2;
            lastDisplaySwitch = now;
        }
    }
}

void forceStop() {
    printer.setTemp = 0;
    analogWrite(heaterPin, 0);
    printer.heaterOn = false;
    digitalWrite(fanPin, LOW);
    printer.fanOn = false;
    displayFrozen = true;
    freezeStartTime = millis();
    showMessage("** Forced STOP **", "");
}


void runTemperatureTask() {
    readTemperature();
    controlHeater();
}

void runInputTask() {
    checkButton();
}

void runDisplayTask() {
    autoSwitchDisplay();
    updateLCD();
}

void runGcodeTask() {
    processGcode();
}




void setup() {
    wdt_disable();
    setupInterrupts();
    initButton(buttonPin);

    pinMode(heaterPin, OUTPUT);
    pinMode(fanPin, OUTPUT);
#ifdef ENABLE_BUZZER
    pinMode(buzzerPin, OUTPUT);
#endif
    pinMode(motorEnablePin, OUTPUT);
    digitalWrite(motorEnablePin, HIGH);
    lcd.init();
    lcd.backlight();
#ifndef ENABLE_AXIS_CYCLE_TEST
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    delay(1000);
    lcd.clear();
    lastDisplaySwitch = millis();
#endif

    Serial.begin(9600);
    loadSettingsFromEEPROM();
    wdt_enable(WDTO_4S);
#ifdef ENABLE_BUTTON_MENU_TEST
    testMenuSetup();
#endif
#ifdef ENABLE_AXIS_CYCLE_TEST
    axisTestSetup();
#endif
}

void loop() {
#ifdef ENABLE_BUTTON_MENU_TEST
    wdt_reset();
    testMenuLoop();
    return;
#endif
#ifdef ENABLE_AXIS_CYCLE_TEST
    wdt_reset();
    axisTestLoop();
    return;
#endif
    wdt_reset();
    unsigned long now = millis();
    if (now - lastLoopTime >= loopInterval) {
        lastLoopTime = now;
        runTemperatureTask();
        runInputTask();
        runDisplayTask();
        runGcodeTask();
    }
}

/* ===== Begin inlined source files ===== */

// --- pins.cpp ---
const int stepPinX = 2;
const int dirPinX  = 5;
const int stepPinY = 3;
const int dirPinY  = 6;
const int stepPinZ = 4;
const int dirPinZ  = 7;
const int stepPinE = 12;
const int dirPinE  = 13;
const int fanPin    = 9;
const int heaterPin = 10;
const int tempPin   = A3;
#ifndef BUZZER_PIN
#define BUZZER_PIN 8
#endif
const int buzzerPin = BUZZER_PIN;
const int motorEnablePin = 8;
const int buttonPin = 11;
const int endstopX = A0;
const int endstopY = A1;
const int endstopZ = A2;
int eMaxSteps = 1000;

// --- state.cpp ---
PrinterState printer;

void resetPrinterState() {
    printer.setTemp = 0.0f;
    printer.currentTemp = 0.0f;
    printer.rawTemp = 0;
    printer.tempError = false;
    printer.tempErrorNotified = false;
    printer.heatDoneBeeped = false;

    printer.posX = printer.posY = printer.posZ = printer.posE = 0;
    printer.eStart = 0;
    printer.eTotal = -1;
    printer.progress = 0;
    printer.eStartSynced = false;

    printer.fanOn = false;
    printer.fanStarted = false;
    printer.fanForced = false;
    printer.heaterOn = false;

    printer.movingAxis = ' ';
    printer.movingDir = 0;
    printer.lastMoveTime = 0;

    printer.Kp = 20.0f;
    printer.Ki = 1.0f;
    printer.Kd = 50.0f;
    printer.integral = 0.0f;
    printer.previousError = 0.0f;
    printer.lastTime = millis();

    printer.currentTune = TUNE_MARIO;
}

void updateProgress() {
    if (printer.eTotal > 0) {
        if (printer.eStart > printer.posE) {
            printer.eStart = printer.posE;
        }
        long delta = printer.posE - printer.eStart;
        if (delta >= printer.eTotal) {
            printer.progress = 100;
            printer.eTotal = 0;
        } else if (delta > 0) {
            printer.progress = (int)(delta * 100L / printer.eTotal);
        }
    }
}

// --- button.cpp ---
static bool lastState = HIGH;
static bool justFlag = false;
static unsigned long pressStart = 0;
static unsigned long lastDebounce = 0;
static unsigned long debounceMsVal = 25;

void initButton(int pin, unsigned long debounceMs) {
    pinMode(pin, INPUT_PULLUP);
    lastState = digitalRead(pin);
    debounceMsVal = debounceMs;
}

void updateButton() {
    if (!buttonTriggered) return;
    buttonTriggered = false;
    bool state = digitalRead(buttonPin);
    unsigned long now = millis();
    if (state != lastState && (now - lastDebounce >= debounceMsVal)) {
        if (state == LOW) {
            pressStart = now;
            justFlag = true;
        }
        lastState = state;
        lastDebounce = now;
    }
}

bool isPressed() { return lastState == LOW; }

static bool consumeJust() {
    if (justFlag) { justFlag = false; return true; }
    return false;
}

bool justPressed() { return consumeJust(); }

bool longPressed(unsigned long ms) { return isPressed() && (millis() - pressStart >= ms); }

bool doublePressed(unsigned long ms) {
    static unsigned long lastSingle = 0;
    static const unsigned long debounceGap = 50;
    if (consumeJust()) {
        unsigned long now = millis();
        if (now - lastSingle <= debounceGap) {
            lastSingle = now;
            return false;
        }
        if (now - lastSingle <= ms) {
            lastSingle = 0;
            return true;
        }
        lastSingle = now;
    }
    return false;
}

// --- interrupts.cpp ---
volatile bool buttonTriggered = false;

void onButtonInterrupt() { buttonTriggered = true; }

void setupInterrupts() {
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(endstopX, INPUT_PULLUP);
    pinMode(endstopY, INPUT_PULLUP);
    pinMode(endstopZ, INPUT_PULLUP);
    enableInterrupt(buttonPin, onButtonInterrupt, CHANGE);
}

// --- motion.cpp ---
extern void checkButton();

static bool isPhysicalAxis(char axis) { return !(axis == 'E' && printer.eTotal < 0); }

static long calculateSteps(char axis, long currentPos, int &distance, float spm) {
    if (axis == 'E' && distance > 0) {
        if (currentPos + distance > eMaxSteps) {
            distance = eMaxSteps - currentPos;
            if (distance <= 0) return 0;
        }
    }
    return lroundf(fabs(distance * spm));
}

static void setMotorDirection(int dirPin, int distance) {
    int dir = (distance >= 0) ? HIGH : LOW;
    digitalWrite(dirPin, dir);
}

static void moveWithAccel(int stepPin, long steps, long minDelay) {
    const int ACCEL_STEPS = 50;
    long startDelay = minDelay * 2;
    int rampSteps = min(steps / 2, (long)ACCEL_STEPS);
    long delayDelta = rampSteps > 0 ? (startDelay - minDelay) / rampSteps : 0;
    long currentDelay = startDelay;

    unsigned long lastPoll = millis();
    for (long i = 0; i < steps; i++) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(stepPin, LOW);

        unsigned long now = millis();
        if (now - lastPoll >= 50) {
            lastPoll = now;
#ifndef ENABLE_AXIS_CYCLE_TEST
            checkButton();
#endif
            wdt_reset();
        }

        delayMicroseconds(currentDelay);

        if (rampSteps > 0) {
            if (i < rampSteps) {
                currentDelay = max(minDelay, currentDelay - delayDelta);
            } else if (i >= steps - rampSteps) {
                currentDelay = min(startDelay, currentDelay + delayDelta);
            }
        }
    }
}

void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate, char axis) {
    int distance = useAbsolute ? target - pos : target;

    float spm = stepsPerMM_X;
    if (axis == 'Y') spm = stepsPerMM_Y;
    else if (axis == 'Z') spm = stepsPerMM_Z;
    else if (axis == 'E') spm = stepsPerMM_E;

    long steps = calculateSteps(axis, pos, distance, spm);
    if (steps == 0) { pos += distance; return; }

    if (isPhysicalAxis(axis)) {
        digitalWrite(motorEnablePin, LOW);
        setMotorDirection(dirPin, distance);

        long minDelay = (long)(60000000.0 / (feedrate * spm));
        minDelay = max(50L, minDelay);

        moveWithAccel(stepPin, steps, minDelay);

        digitalWrite(motorEnablePin, HIGH);
    }

    pos += distance;
}

#ifdef ENABLE_HOMING
void homeAxis(int stepPin, int dirPin, volatile bool &triggered, const char* label) {
    triggered = false;
    digitalWrite(motorEnablePin, LOW);
    digitalWrite(dirPin, LOW);
    while (!triggered) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(800);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(800);
    }
    digitalWrite(motorEnablePin, HIGH);
    triggered = false;
    sendOk(String(label) + " Homed");
}
#endif

// --- temp_control.cpp ---
float readThermistor(int pin) {
#ifdef DEBUG_INPUT
    static float simTemp = 25.0f;
    if (printer.setTemp > simTemp) {
        simTemp += 1.0f;
        if (simTemp > printer.setTemp) simTemp = printer.setTemp;
    } else if (printer.setTemp <= 0.0f && simTemp > 25.0f) {
        simTemp -= 1.0f;
        if (simTemp < 25.0f) simTemp = 25.0f;
    }
    printer.rawTemp = (int)(simTemp * 2);
    return simTemp;
#else
    int raw = analogRead(pin);
    printer.rawTemp = raw;
    float voltage = raw * 5.0f / 1023.0f;
    if (voltage <= 0.0f) {
        return -1000.0f;
    }
    float resistance = (5.0f - voltage) * SERIES_RESISTOR / voltage;
    float tempK = 1.0f / (log(resistance / THERMISTOR_NOMINAL) / BCOEFFICIENT +
                         1.0f / (TEMPERATURE_NOMINAL + 273.15f));
    return tempK - 273.15f;
#endif
}

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
        Serial.print(F("Thermistor ADC:"));
        Serial.print(printer.rawTemp);
        Serial.print(F(" V:"));
        Serial.print(voltage, 3);
        Serial.println(F("V"));
        lastLog = now;
    }

    if (printer.currentTemp < -10 || printer.currentTemp > 300) {
        printer.tempError = true;
        printer.tempErrorNotified = false;
        printer.setTemp = 0;
#ifndef DEBUG_INPUT
        analogWrite(heaterPin, 0);
        digitalWrite(fanPin, LOW);
#endif
        printer.heaterOn = false;
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
        float elapsed = (now - printer.lastTime) / 1000.0f;
        elapsed = max(elapsed, 0.001f);
        printer.lastTime = now;

        float error = printer.setTemp - printer.currentTemp;
        printer.integral += error * elapsed;
        float derivative = (error - printer.previousError) / elapsed;
        printer.previousError = error;

        float output = printer.Kp * error + printer.Ki * printer.integral + printer.Kd * derivative;
        output = constrain(output, 0, 255);
#ifndef DEBUG_INPUT
        analogWrite(heaterPin, (int)output);
#endif
        printer.heaterOn = output > 0;

        if (printer.currentTemp >= 50 && !printer.fanStarted && !printer.fanForced) {
#ifndef DEBUG_INPUT
            digitalWrite(fanPin, HIGH);
#endif
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
#ifndef DEBUG_INPUT
        analogWrite(heaterPin, 0);
#endif
        printer.heaterOn = false;
        if (!printer.fanForced) {
#ifndef DEBUG_INPUT
            digitalWrite(fanPin, LOW);
#endif
            printer.fanOn = false;
            printer.fanStarted = false;
        }
        printer.heatDoneBeeped = false;
        heatStableStart = 0;
    }
}

// --- tunes.cpp ---
extern LiquidCrystal_I2C lcd;

static const int marioNotes[] = {262,262,0,262,0,196,262,0,0,0,294,0,330};
static const int marioDur[]   = {200,200,100,200,100,400,400,100,100,100,400,100,600};
static const int canonNotes[] = {392,440,494,523,587,523,494,440,392};
static const int canonDur[]   = {250,250,250,250,250,250,250,250,500};
static const int starNotes[]  = {440,440,440,349,523,440,349,523,440};
static const int starDur[]    = {300,300,300,200,600,300,200,600,800};
static const int tetrisNotes[] = {659,494,523,587,523,494,440,440,523,659,587,523,494,523,587,659};
static const int tetrisDur[]   = {150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150};

#ifdef ENABLE_BUZZER
void playTune(int tune) {
    const int *notes = marioNotes;
    const int *durs = marioDur;
    int length = sizeof(marioNotes)/sizeof(int);
    const char *label = "Mario";

    switch (tune) {
        case TUNE_CANON:
            notes = canonNotes;
            durs = canonDur;
            length = sizeof(canonNotes)/sizeof(int);
            label = "Canon";
            break;
        case TUNE_STAR_WARS:
            notes = starNotes;
            durs = starDur;
            length = sizeof(starNotes)/sizeof(int);
            label = "StarWars";
            break;
        case TUNE_TETRIS:
            notes = tetrisNotes;
            durs = tetrisDur;
            length = sizeof(tetrisNotes)/sizeof(int);
            label = "Tetris";
            break;
        case TUNE_MARIO:
        default:
            break;
    }

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Tune: ");
    lcd.print(label);
    lcd.setCursor(0,1);

    for (int i = 0; i < length; i++) {
        if (notes[i] == 0) {
            noTone(buzzerPin);
        } else {
            tone(buzzerPin, notes[i], durs[i]);
        }
        delay(durs[i] + 50);
        wdt_reset();
        lcd.print((char)255);
    }
    noTone(buzzerPin);
    delay(500);
    wdt_reset();
    lcd.clear();
}
#endif

// --- gcode.cpp ---
extern bool useAbsolute;
extern int currentFeedrate;
extern const int fanPin;
extern const int stepPinX, dirPinX, stepPinY, dirPinY, stepPinZ, dirPinZ, stepPinE, dirPinE;
extern volatile bool endstopXTriggered, endstopYTriggered, endstopZTriggered;
extern void playTune(int tune);
extern void saveSettingsToEEPROM();
extern void updateProgress();
extern float stepsPerMM_X, stepsPerMM_Y, stepsPerMM_Z, stepsPerMM_E;
extern char lastDisplayContent[33];
extern void showMessage(const char*, const char*);

static void displayM503LCD() {
    for (int t = 5; t > 0; --t) {
        char line1[17];
        char line2[17];
        char kp[8], ki[8], kd[8];
        dtostrf(printer.Kp, 1, 0, kp);
        dtostrf(printer.Ki, 1, 0, ki);
        dtostrf(printer.Kd, 1, 0, kd);
        snprintf(line1, sizeof(line1), "P%s I%s D%s %d", kp, ki, kd, t);

        char sx[8], sy[8], sz[8], se[8];
        dtostrf(stepsPerMM_X, 1, 0, sx);
        dtostrf(stepsPerMM_Y, 1, 0, sy);
        dtostrf(stepsPerMM_Z, 1, 0, sz);
        dtostrf(stepsPerMM_E, 1, 0, se);
        snprintf(line2, sizeof(line2), "X%s Y%s Z%s E%s", sx, sy, sz, se);
        showMessage(line1, line2);
        delay(1000);
        wdt_reset();
    }
    memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
}

#ifdef DEBUG_INPUT
static const char *debugCommands[] = {
    "M104 S200",
    "M290 E100",
    "G90",
    "G92 X0 Y0 Z0 E0",
    "G1 X10 Y10 F800",
    "G1 E50 F600",
    "G1 X20 Y20 F800",
    "G1 E100 F600",
    "M105",
    "M400",
    "M401 S1"
};
static const int debugCommandCount = sizeof(debugCommands) / sizeof(debugCommands[0]);
static int debugIndex = 0;
#endif

static String getGcodeInput() {
#ifdef DEBUG_INPUT
    if (debugIndex < debugCommandCount) {
        String cmd(debugCommands[debugIndex++]);
        Serial.print(F("DBG> "));
        Serial.println(cmd);
        return cmd;
    }
#endif
    if (Serial.available()) {
        return Serial.readStringUntil('\n');
    }
    return String();
}

void processGcode() {
    String gcode = getGcodeInput();
    if (gcode.length()) {
        gcode.trim();

        if (gcode.startsWith("G90")) {
            useAbsolute = true;
            sendOk(F("G90 Absolute mode"));
        } else if (gcode.startsWith("G91")) {
            useAbsolute = false;
            sendOk(F("G91 Relative mode"));
        } else if (gcode.startsWith("G92")) {
            if (gcode.indexOf('X') != -1) printer.posX = gcode.substring(gcode.indexOf('X') + 1).toInt();
            if (gcode.indexOf('Y') != -1) printer.posY = gcode.substring(gcode.indexOf('Y') + 1).toInt();
            if (gcode.indexOf('Z') != -1) printer.posZ = gcode.substring(gcode.indexOf('Z') + 1).toInt();
            if (gcode.indexOf('E') != -1) {
                printer.posE = gcode.substring(gcode.indexOf('E') + 1).toInt();
                printer.eStart = printer.posE;
                sendOk(F("G92 E origin reset"));
            } else {
                sendOk(F("G92 Origin set"));
            }
        } else if (gcode.startsWith("M104")) {
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float target = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(target)) {
                    printer.setTemp = target;
                    printer.heatDoneBeeped = false;
                    sendOk(String("Set temperature to ") + printer.setTemp);
                }
            }
        } else if (gcode.startsWith("M105")) {
            String msg = String("T:") + String(printer.currentTemp, 1) + " /" + String(printer.setTemp, 1) + " B:0.0 /0.0";
            sendOk(msg);
        } else if (gcode.startsWith("M106")) {
            printer.fanForced = true;
            digitalWrite(fanPin, HIGH);
            printer.fanOn = true;
            sendOk(F("Fan ON"));
        } else if (gcode.startsWith("M107")) {
            printer.fanForced = false;
            digitalWrite(fanPin, LOW);
            printer.fanOn = false;
            printer.fanStarted = false;
            sendOk(F("Fan OFF"));
        } else if (gcode.startsWith("M301")) {
            int pIndex = gcode.indexOf('P');
            int iIndex = gcode.indexOf('I');
            int dIndex = gcode.indexOf('D');

            float temp;
            if (pIndex != -1) {
                temp = gcode.substring(pIndex + 1, (iIndex != -1 ? iIndex : gcode.length())).toFloat();
                if (!isnan(temp)) printer.Kp = temp;
            }
            if (iIndex != -1) {
                temp = gcode.substring(iIndex + 1, (dIndex != -1 ? dIndex : gcode.length())).toFloat();
                if (!isnan(temp)) printer.Ki = temp;
            }
            if (dIndex != -1) {
                temp = gcode.substring(dIndex + 1).toFloat();
                if (!isnan(temp)) printer.Kd = temp;
            }

            saveSettingsToEEPROM();
            String pidMsg = String("Kp:") + printer.Kp + " Ki:" + printer.Ki + " Kd:" + printer.Kd;
            sendOk(pidMsg);
        } else if (gcode.startsWith("M400")) {
#ifdef ENABLE_BUZZER
            playTune(printer.currentTune);
#endif
            sendOk(F("Print Complete"));
        } else if (gcode.startsWith("M401")) {
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                int val = gcode.substring(sIndex + 1).toInt();
                if (val >= 0 && val < TUNE_COUNT) {
                    printer.currentTune = val;
                    sendOk(String("Tune set to ") + val);
                } else {
                    sendOk(F("Invalid tune"));
                }
            } else {
                sendOk(String("Current tune: ") + printer.currentTune);
            }
        } else if (gcode.startsWith("M92")) {
            int idx;
            float val;

            idx = gcode.indexOf('X');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_X = val;
            }
            idx = gcode.indexOf('Y');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_Y = val;
            }
            idx = gcode.indexOf('Z');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_Z = val;
            }
            idx = gcode.indexOf('E');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_E = val;
            }
            sendOk(F("Steps per mm updated"));
        } else if (gcode.startsWith("M290")) {
            int eIndex = gcode.indexOf('E');
            if (eIndex != -1) {
                long val = gcode.substring(eIndex + 1).toInt();
                if (val > 0) {
                    printer.eTotal = val;
                    printer.eStart = printer.posE;
                    printer.eStartSynced = true;
                    printer.progress = 0;
                    sendOk(String("eTotal set to ") + printer.eTotal);
                }
            }
        } else if (gcode.startsWith("M500")) {
            saveSettingsToEEPROM();
            sendOk(F("Settings saved"));
        } else if (gcode.startsWith("M503")) {
            sendOk(F("Current settings"));
            Serial.print(F("Kp = ")); Serial.println(printer.Kp);
            Serial.print(F("Ki = ")); Serial.println(printer.Ki);
            Serial.print(F("Kd = ")); Serial.println(printer.Kd);
            Serial.print(F("Steps/mm X:")); Serial.println(stepsPerMM_X);
            Serial.print(F("Steps/mm Y:")); Serial.println(stepsPerMM_Y);
            Serial.print(F("Steps/mm Z:")); Serial.println(stepsPerMM_Z);
            Serial.print(F("Steps/mm E:")); Serial.println(stepsPerMM_E);
            displayM503LCD();
        } else if (gcode.startsWith("G1")) {
            int fIndex = gcode.indexOf('F');
            if (fIndex != -1) {
                int fend = gcode.indexOf(' ', fIndex);
                String fStr = (fend != -1) ? gcode.substring(fIndex + 1, fend) : gcode.substring(fIndex + 1);
                int parsed = fStr.toInt();
                if (parsed > 0) currentFeedrate = parsed;
            }
            handleG1Axis('X', stepPinX, dirPinX, printer.posX, gcode);
            handleG1Axis('Y', stepPinY, dirPinY, printer.posY, gcode);
            handleG1Axis('Z', stepPinZ, dirPinZ, printer.posZ, gcode);
            handleG1Axis('E', stepPinE, dirPinE, printer.posE, gcode);
        } else if (gcode.startsWith("G28")) {
#ifdef ENABLE_HOMING
            homeAxis(stepPinX, dirPinX, endstopXTriggered, "X");
            homeAxis(stepPinY, dirPinY, endstopYTriggered, "Y");
            homeAxis(stepPinZ, dirPinZ, endstopZTriggered, "Z");
#else
            sendOk(F("Homing disabled"));
#endif
        } else {
            Serial.print(F("error: Unknown command "));
            Serial.println(gcode);
        }
    }
}

void handleG1Axis(char axis, int stepPin, int dirPin, long& pos, String& gcode) {
    int idx = gcode.indexOf(axis);
    if (idx != -1) {
        int end = gcode.indexOf(' ', idx);
        String valStr = (end != -1) ? gcode.substring(idx + 1, end) : gcode.substring(idx + 1);
        int val = valStr.toInt();

        if (&pos == &printer.posE) {
            if (printer.eTotal == -1) {
                Serial.println(F("error: eTotal not set"));
                sendOk();
                return;
            }
            if (!printer.eStartSynced) {
                printer.eStart = printer.posE;
                printer.eStartSynced = true;
            }
            updateProgress();
#ifdef DEBUG_INPUT
            Serial.print(F("Progress: "));
            Serial.print(printer.progress);
            Serial.println('%');
#endif
        }

        int distance = useAbsolute ? val - pos : val;
        printer.movingAxis = axis;
        printer.movingDir = (distance >= 0) ? 1 : -1;
        printer.lastMoveTime = millis();

        moveAxis(stepPin, dirPin, pos, val, currentFeedrate, axis);

        String moveMsg = String("Move ") + axis + " to " + (useAbsolute ? val : pos);
        sendOk(moveMsg);
    }
}

// --- test_modes.cpp ---
extern LiquidCrystal_I2C lcd;
extern int displayMode;
extern void showMessage(const char*, const char*);
extern void checkButton();
extern void updateLCD();

#ifdef ENABLE_BUTTON_MENU_TEST
static int dummyProgress = 0;
static float dummyTemp = 25.0;

void testMenuSetup() { showMessage("Menu Test", "Press Button"); }

void testMenuLoop() {
    dummyProgress = (dummyProgress + 1) % 101;
    dummyTemp += 0.1;
    if (dummyTemp > 60) dummyTemp = 25.0;

    printer.currentTemp = dummyTemp;
    printer.progress = dummyProgress;

    checkButton();
    updateLCD();
}
#endif

#ifdef ENABLE_AXIS_CYCLE_TEST
static int currentAxis = 0;
static bool moving = false;
static const char axisChars[] = {'X','Y','Z','E'};

void axisTestSetup() {
    showMessage("Axis Test", "Press Button");
    sendOk(F("Axis cycle test ready"));
}

void axisTestLoop() {
    updateButton();
    if (justPressed()) {
        if (moving) {
            moving = false;
            currentAxis = (currentAxis + 1) % 4;
            char buf[17];
            snprintf(buf, sizeof(buf), "Next: %c axis", axisChars[currentAxis]);
            showMessage(buf, "Press to Start");
            sendOk(String(buf));
        } else {
            moving = true;
            char buf[17];
            snprintf(buf, sizeof(buf), "Moving %c axis", axisChars[currentAxis]);
            showMessage(buf, "Press to Stop");
            sendOk(String(buf));
        }
    }

    if (moving) {
        long* posPtr;
        int stepPin, dirPin;
        char axis;
        switch (currentAxis) {
            case 0: axis = 'X'; posPtr = &printer.posX; stepPin = stepPinX; dirPin = dirPinX; break;
            case 1: axis = 'Y'; posPtr = &printer.posY; stepPin = stepPinY; dirPin = dirPinY; break;
            case 2: axis = 'Z'; posPtr = &printer.posZ; stepPin = stepPinZ; dirPin = dirPinZ; break;
            default: axis = 'E'; posPtr = &printer.posE; stepPin = stepPinE; dirPin = dirPinE; break;
        }
        moveAxis(stepPin, dirPin, *posPtr, *posPtr + 1, 600, axis);
    }
}
#endif

/* ===== End inlined source files ===== */

