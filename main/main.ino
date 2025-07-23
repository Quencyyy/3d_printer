// Uncomment to feed predefined G-code without host software
//#define DEBUG_INPUT

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include "button.h"
#include <EEPROM.h>
#include "pins.h"
#include "temp_control.h"
#include "gcode.h"
#include "tunes.h"
#include "state.h"
#include "motion.h"
#include "interrupts.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

int currentFeedrate = 1200;  // 預設速度 mm/min
unsigned long heatStableStart = 0;
const unsigned long stableHoldTime = 3000;

bool useAbsolute = true;
bool useRelativeE = false;

float stepsPerMM_X = 25.0;
float stepsPerMM_Y = 25.0;
float stepsPerMM_Z = 25.0;
float stepsPerMM_E = 25.0;

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

    // Validate values in case EEPROM has never been written
    if (!isfinite(printer.Kp) || !isfinite(printer.Ki) || !isfinite(printer.Kd)) {
        printer.Kp = 20.0f;
        printer.Ki = 1.0f;
        printer.Kd = 50.0f;
    }
    if (!isfinite(stepsPerMM_X)) stepsPerMM_X = 25.0f;
    if (!isfinite(stepsPerMM_Y)) stepsPerMM_Y = 25.0f;
    if (!isfinite(stepsPerMM_Z)) stepsPerMM_Z = 25.0f;
    if (!isfinite(stepsPerMM_E)) stepsPerMM_E = 25.0f;
    if (!isfinite(printer.setTemp) || printer.setTemp < 0 || printer.setTemp > 300) {
        printer.setTemp = 0.0f;
    }
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
    lcd.print(' ');
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
#ifdef ENABLE_BUZZER
    pinMode(buzzerPin, OUTPUT);
#endif
    pinMode(motorEnablePin, OUTPUT);
    
    // Configure stepper driver pins
    pinMode(stepPinX, OUTPUT);
    pinMode(dirPinX, OUTPUT);
    pinMode(stepPinY, OUTPUT);
    pinMode(dirPinY, OUTPUT);
    pinMode(stepPinZ, OUTPUT);
    pinMode(dirPinZ, OUTPUT);
    pinMode(stepPinE, OUTPUT);
    pinMode(dirPinE, OUTPUT);

    digitalWrite(motorEnablePin, HIGH);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    delay(1000);
    lcd.clear();
    lastDisplaySwitch = millis();

    Serial.begin(9600);
    resetPrinterState();
    loadSettingsFromEEPROM();
    wdt_enable(WDTO_4S);
}

void loop() {
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

