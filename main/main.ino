// Simulation flags are defined in config.h
#include "config.h"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
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

bool useAbsoluteXYZ = true;
bool useRelativeE = false;

float stepsPerMM_X = 25.0;
float stepsPerMM_Y = 25.0;
float stepsPerMM_Z = 25.0;
float stepsPerMM_E = 25.0;

int displayMode = 0;
unsigned long lastPressTime = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long autoSwitchDelay = 30000;
// Immediately show the idle screen after startup
const unsigned long idleSwitchDelay = 0;
bool isLongPress = false;
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
        printer.Kp = 0.6f;
        printer.Ki = 0.05f;
        printer.Kd = 1.2f;
    }
    if (!isfinite(stepsPerMM_X)) stepsPerMM_X = 25.0f;
    if (!isfinite(stepsPerMM_Y)) stepsPerMM_Y = 25.0f;
    if (!isfinite(stepsPerMM_Z)) stepsPerMM_Z = 25.0f;
    if (!isfinite(stepsPerMM_E)) stepsPerMM_E = 25.0f;
    if (!isfinite(printer.setTemp) || printer.setTemp < 0 || printer.setTemp > 300) {
        printer.setTemp = 0.0f;
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

void displayProgressScreen() {
    if (printer.eTotal == 0) {
        showMessage("Print Complete", "Press Button");
        return;
    }

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
    snprintf(line1, sizeof(line1), "[%s]%3d%%", bar, printer.progress);

    char cur[8];
    dtostrf(printer.currentTemp, 4, 1, cur);
    char line2[17];
    snprintf(line2, sizeof(line2), "T:%s%cC", cur, 223);
    showMessage(line1, line2);
}

void displayCoordScreen() {
    char buf1[17], buf2[17];
    if (useAbsoluteXYZ) {
        char xbuf[8], ybuf[8], zbuf[8], ebuf[8];
        dtostrf(printer.posX, 4, 1, xbuf);
        dtostrf(printer.posY, 4, 1, ybuf);
        dtostrf(printer.posZ, 4, 1, zbuf);
        dtostrf(printer.posE, 4, 1, ebuf);
        snprintf(buf1, sizeof(buf1), "X%s Y%s", xbuf, ybuf);
        snprintf(buf2, sizeof(buf2), "Z%s E%s", zbuf, ebuf);
    } else {
        long rx = printer.signX * lroundf(printer.remStepX / stepsPerMM_X);
        long ry = printer.signY * lroundf(printer.remStepY / stepsPerMM_Y);
        long rz = printer.signZ * lroundf(printer.remStepZ / stepsPerMM_Z);
        long re = printer.signE * lroundf(printer.remStepE / stepsPerMM_E);
        snprintf(buf1, sizeof(buf1), "%ld %ld %ld %ld", rx, ry, rz, re);
        char nx[8], ny[8], nz[8], ne[8];
        dtostrf(printer.nextX, 4, 1, nx);
        dtostrf(printer.nextY, 4, 1, ny);
        dtostrf(printer.nextZ, 4, 1, nz);
        dtostrf(printer.nextE, 4, 1, ne);
        snprintf(buf2, sizeof(buf2), "%s %s %s %s", nx, ny, nz, ne);
    }
    showMessage(buf1, buf2);
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
    if (printer.paused) {
        showMessage("** Paused **", "Press Button");
        return;
    }
    static int animPos = 0;
    static const char anim[] = "|/-\\";

    bool idle = (displayMode == 0 &&
                 printer.eTotal == -1 &&
                 millis() - lastPressTime >= idleSwitchDelay);
    if (idle) {
        displayIdleScreen(animPos);
    } else if (displayMode == 0) {
        displayProgressScreen();
    } else {
        displayCoordScreen();
    }

    bool moving = (millis() - printer.lastMoveTime) < 1000 && printer.movingAxis != ' ';
    lcd.setCursor(11, 1);
    lcd.print(printer.heaterOn ? 'H' : ' ');
    lcd.setCursor(12, 1);
    lcd.print(useAbsoluteXYZ ? "ABS" : "REL");
    lcd.setCursor(15, 1);
    if (moving) {
        lcd.print(printer.movingDir > 0 ? '>' : '<');
    } else {
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

    if (printer.paused) {
        if (justPressed()) {
            printer.paused = false;
            showMessage("Resuming", "");
            delay(300);
            memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
        }
        prevState = state;
        return;
    }

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


    if (justPressed()) {
        pressStartTime = now;
    }

    if (state && !isLongPress && now - pressStartTime > 50) {
        if (longPressed(3000)) {
            enterPauseMode();
            sendOk(F("Paused"));
            isLongPress = true;
        }
    }

    if (!state && prevState) {
        if (!isLongPress) {
            displayMode = (displayMode + 1) % 2;
            lastDisplaySwitch = now;
        }
        isLongPress = false;
        lastPressTime = now;
    }

    prevState = state;
}

void autoSwitchDisplay() {
    if (displayMode != 0 && printer.progress < 100 && printer.eStartSynced && printer.eTotal > 0) {
        unsigned long now = millis();
        if (now - lastPressTime >= autoSwitchDelay && now - lastDisplaySwitch >= autoSwitchDelay) {
            displayMode = 0;
            lastDisplaySwitch = now;
        }
    }
}

void enterPauseMode() {
    printer.paused = true;
    showMessage("** Paused **", "Press Button");
    memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
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
    if (!printer.paused) {
        processGcode();
    }
}




void setup() {
    setupInterrupts();
    initButton(buttonPin);

    pinMode(heaterPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
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
}

void loop() {
    unsigned long now = millis();
    if (now - lastLoopTime >= loopInterval) {
        lastLoopTime = now;
        runTemperatureTask();
        runInputTask();
        runDisplayTask();
        runGcodeTask();
    }
}

