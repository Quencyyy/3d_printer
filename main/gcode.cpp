// gcode.cpp
#include "gcode.h"
#include "tunes.h"
#include "state.h"
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>

// Unified serial response helper
void sendOk(const String &msg = "") {
    Serial.print(F("ok"));
    if (msg.length()) {
        Serial.print(' ');
        Serial.print(msg);
    }
    Serial.println();
}

// 外部變數宣告
extern bool useAbsolute;
extern int currentFeedrate;
extern const int stepPinX, dirPinX, stepPinY, dirPinY, stepPinZ, dirPinZ, stepPinE, dirPinE;
extern const int endstopX, endstopY, endstopZ;
extern void playTune(int tune);
extern void saveSettingsToEEPROM();
extern void updateProgress();
extern float stepsPerMM_X, stepsPerMM_Y, stepsPerMM_Z, stepsPerMM_E;
extern LiquidCrystal_I2C lcd;
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

        if (gcode.startsWith("G90")) {          // G90 - 進入絕對座標模式

            useAbsolute = true;
            sendOk(F("G90 Absolute mode"));
        } else if (gcode.startsWith("G91")) {   // G91 - 進入相對座標模式
            useAbsolute = false;
            sendOk(F("G91 Relative mode"));
        } else if (gcode.startsWith("G92")) {   // G92 - 手動設定目前座標（包含 E 也會同步進度 eStart）
            if (gcode.indexOf('X') != -1) printer.posX = gcode.substring(gcode.indexOf('X') + 1).toInt();
            if (gcode.indexOf('Y') != -1) printer.posY = gcode.substring(gcode.indexOf('Y') + 1).toInt();
            if (gcode.indexOf('Z') != -1) printer.posZ = gcode.substring(gcode.indexOf('Z') + 1).toInt();
            if (gcode.indexOf('E') != -1) {
                printer.posE = gcode.substring(gcode.indexOf('E') + 1).toInt();
                printer.eStart = printer.posE;  // 同步進度起點，避免重設座標後估算錯誤
                sendOk(F("G92 E origin reset"));
            } else {
                sendOk(F("G92 Origin set"));
            }
        } else if (gcode.startsWith("M104")) {  // M104 Snnn - 設定加熱目標溫度（不等待）
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float target = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(target)) {
                    printer.setTemp = target;
                    printer.heatDoneBeeped = false;
                    sendOk(String("Set temperature to ") + printer.setTemp);
                }
            }
        } else if (gcode.startsWith("M105")) {  // M105 - 回報目前溫度
            String msg = String("T:") + String(printer.currentTemp, 1) + " /" + String(printer.setTemp, 1) + " B:0.0 /0.0";
            sendOk(msg);
        } else if (gcode.startsWith("M301")) {  // M301 Pn In Dn - 設定 PID 控制參數
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
        } else if (gcode.startsWith("M400")) {  // M400 - 播放選定音樂，列印完成提示
#ifdef ENABLE_BUZZER
            playTune(printer.currentTune);
#endif
            sendOk(F("Print Complete"));
        } else if (gcode.startsWith("M401")) {  // M401 Sn - 設定列印完成音樂
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
        } else if (gcode.startsWith("M92")) {   // M92 - 設定各軸 steps/mm
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
        } else if (gcode.startsWith("M290")) { // M290 En - 設定進度總量
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
        } else if (gcode.startsWith("M500")) {  // M500 - 儲存設定到 EEPROM
            saveSettingsToEEPROM();
            sendOk(F("Settings saved"));
        } else if (gcode.startsWith("M503")) {  // M503 - 印出目前參數
            sendOk(F("Current settings"));
            Serial.print(F("Kp = ")); Serial.println(printer.Kp);
            Serial.print(F("Ki = ")); Serial.println(printer.Ki);
            Serial.print(F("Kd = ")); Serial.println(printer.Kd);
            Serial.print(F("Steps/mm X:")); Serial.println(stepsPerMM_X);
            Serial.print(F("Steps/mm Y:")); Serial.println(stepsPerMM_Y);
            Serial.print(F("Steps/mm Z:")); Serial.println(stepsPerMM_Z);
            Serial.print(F("Steps/mm E:")); Serial.println(stepsPerMM_E);
            displayM503LCD();
        } else if (gcode.startsWith("G1")) {    // G1 Xn Yn Zn En - 執行軸移動
                int fIndex = gcode.indexOf('F');
                if (fIndex != -1) {
                    int fend = gcode.indexOf(' ', fIndex);
                    String fStr = (fend != -1) ? gcode.substring(fIndex + 1, fend) : gcode.substring(fIndex + 1);
                    int parsed = fStr.toInt();
                    if (parsed > 0)
                        currentFeedrate = parsed;
                }
                handleG1Axis('X', stepPinX, dirPinX, printer.posX, gcode);
                handleG1Axis('Y', stepPinY, dirPinY, printer.posY, gcode);
                handleG1Axis('Z', stepPinZ, dirPinZ, printer.posZ, gcode);
                handleG1Axis('E', stepPinE, dirPinE, printer.posE, gcode);
        } else if (gcode.startsWith("G28")) {   // G28 - 執行回原點
            homeAxis(stepPinX, dirPinX, endstopX, "X");
            homeAxis(stepPinY, dirPinY, endstopY, "Y");
            homeAxis(stepPinZ, dirPinZ, endstopZ, "Z");
        } else {  // 其他未知指令
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

        // E 軸同步判斷與進度更新
        if (&pos == &printer.posE) {
            if (printer.eTotal == -1) {
                Serial.println(F("warning: eTotal not set"));
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

        // 設定移動方向供顯示使用
        int distance = useAbsolute ? val - pos : val;
        printer.movingAxis = axis;
        printer.movingDir = (distance >= 0) ? 1 : -1;
        printer.lastMoveTime = millis();

        // 呼叫含速度的移動
        moveAxis(stepPin, dirPin, pos, val, currentFeedrate, axis);

        String moveMsg = String("Move ") + axis + " to " + (useAbsolute ? val : pos);
        sendOk(moveMsg);
    }
}

