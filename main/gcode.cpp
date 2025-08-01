// gcode.cpp
#include "gcode.h"
#include "tunes.h"
#include "state.h"
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "config.h"

// Unified serial response helpers
void sendOk(const __FlashStringHelper* msg) {
    Serial.print(F("ok"));
    if (msg) {
        Serial.print(' ');
        Serial.print(msg);
    }
    Serial.println();
}

void sendOk(const char* msg) {
    Serial.print(F("ok"));
    if (msg && msg[0]) {
        Serial.print(' ');
        Serial.print(msg);
    }
    Serial.println();
}

void sendOk() {
    Serial.println(F("ok"));
}

// 外部變數宣告
extern bool useAbsoluteXYZ;
extern bool useRelativeE;
extern float feedrateMultiplier;
extern float flowrateMultiplier;
extern int currentFeedrate;
extern const int stepPinX, dirPinX, stepPinY, dirPinY, stepPinZ, dirPinZ, stepPinE, dirPinE;
extern const int endstopX, endstopY, endstopZ;
extern const int motorEnablePin;
extern const int buzzerPin;
extern void playTune(int tune);
extern void saveSettingsToEEPROM();
extern void updateProgress();
extern float stepsPerMM_X, stepsPerMM_Y, stepsPerMM_Z, stepsPerMM_E;
extern LiquidCrystal_I2C lcd;
extern char lastDisplayContent[33];
extern void showMessage(const char*, const char*);

#ifdef SIMULATE_GCODE_INPUT
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
};
static const int debugCommandCount = sizeof(debugCommands) / sizeof(debugCommands[0]);
static int debugIndex = 0;
#endif

static String getGcodeInput() {
#ifdef SIMULATE_GCODE_INPUT
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

static bool isDigitChar(char c) {
    return c >= '0' && c <= '9';
}

// Remove line numbers (Nxxx) and checksums (*xxx) from a raw G-code line
static String cleanGcode(const String &src) {
    String out;
    out.reserve(src.length());
    for (size_t i = 0; i < src.length(); ) {
        char c = src[i];
        if ((c == 'N' || c == 'n') && i + 1 < src.length() &&
            (isDigitChar(src[i + 1]) || src[i + 1] == '-')) {
            i++;
            while (i < src.length() && (isDigitChar(src[i]) || src[i] == '-')) i++;
            if (i < src.length() && src[i] == ' ') i++;
            continue;
        }
        if (c == '*') {
            i++;
            while (i < src.length() && isDigitChar(src[i])) i++;
            continue;
        }
        out += c;
        i++;
    }
    out.trim();
    return out;
}

static void handleMoveCommand(const String &gcode, bool allowExtrude) {
    int fIndex = gcode.indexOf('F');
    if (fIndex != -1) {
        int fend = gcode.indexOf(' ', fIndex);
        String fStr = (fend != -1) ? gcode.substring(fIndex + 1, fend) : gcode.substring(fIndex + 1);
        int parsed = fStr.toInt();
        if (parsed > 0) currentFeedrate = parsed;
    }

    auto parseAxis = [&](char a, float &out)->bool {
        int idx = gcode.indexOf(a);
        if (idx == -1) return false;
        int end = gcode.indexOf(' ', idx);
        String valStr = (end != -1) ? gcode.substring(idx + 1, end) : gcode.substring(idx + 1);
        out = valStr.toFloat();
        return true;
    };

    float tx = 0, ty = 0, tz = 0, te = 0;
    bool hx = parseAxis('X', tx);
    bool hy = parseAxis('Y', ty);
    bool hz = parseAxis('Z', tz);
    bool he = allowExtrude ? parseAxis('E', te) : false;

    if (useAbsoluteXYZ) {
        if (!hx) tx = printer.posX;
        if (!hy) ty = printer.posY;
        if (!hz) tz = printer.posZ;
    }

    if (allowExtrude) {
        if (useRelativeE) {
            if (!he) te = 0;
        } else {
            if (!he) te = printer.posE;
        }
    } else {
        te = useRelativeE ? 0 : printer.posE;
    }

    float distX = useAbsoluteXYZ ? tx - printer.posX : tx;
    float distY = useAbsoluteXYZ ? ty - printer.posY : ty;
    float distZ = useAbsoluteXYZ ? tz - printer.posZ : tz;
    float distE = 0;
    if (allowExtrude) {
        distE = useRelativeE ? te : (useAbsoluteXYZ ? te - printer.posE : te);
        distE *= flowrateMultiplier;
    }

    float targetE = useRelativeE ? distE : (useAbsoluteXYZ ? printer.posE + distE : distE);

    printer.remStepX = lroundf(fabsf(distX * stepsPerMM_X));
    printer.remStepY = lroundf(fabsf(distY * stepsPerMM_Y));
    printer.remStepZ = lroundf(fabsf(distZ * stepsPerMM_Z));
    printer.remStepE = lroundf(fabsf(distE * stepsPerMM_E));
    printer.signX = (distX >= 0) ? 1 : -1;
    printer.signY = (distY >= 0) ? 1 : -1;
    printer.signZ = (distZ >= 0) ? 1 : -1;
    printer.signE = (distE >= 0) ? 1 : -1;

    printer.nextX = useAbsoluteXYZ ? tx : distX;
    printer.nextY = useAbsoluteXYZ ? ty : distY;
    printer.nextZ = useAbsoluteXYZ ? tz : distZ;
    if (allowExtrude) {
        printer.nextE = useRelativeE ? distE : (useAbsoluteXYZ ? targetE : distE);
    } else {
        printer.nextE = useRelativeE ? 0 : printer.posE;
    }
    printer.hasNextMove = true;

    moveAxes(tx, ty, tz, targetE, lroundf(currentFeedrate * feedrateMultiplier));

    printer.hasNextMove = false;
    printer.remStepX = printer.remStepY = printer.remStepZ = printer.remStepE = 0;

    Serial.print(F("ok Move"));
    if (hx) { Serial.print(F(" X")); Serial.print(printer.posX); }
    if (hy) { Serial.print(F(" Y")); Serial.print(printer.posY); }
    if (hz) { Serial.print(F(" Z")); Serial.print(printer.posZ); }
    if (allowExtrude && (he || distE != 0)) { Serial.print(F(" E")); Serial.print(printer.posE); }
    Serial.println();
}

void processGcode() {
    String gcode;
    if (printer.waitingForHeat) {
        if (fabs(printer.currentTemp - printer.setTemp) < 1.0 && printer.heatDoneBeeped) {
            printer.waitingForHeat = false;
            sendOk(F("Target temp reached"));
        }
        gcode = getGcodeInput();
        if (gcode.length()) {
            gcode.trim();
            gcode = cleanGcode(gcode);
            if (gcode.startsWith("M105")) {
                Serial.print(F("ok T:"));
                Serial.print(printer.currentTemp, 1);
                Serial.print(F(" /"));
                Serial.print(printer.setTemp, 1);
                Serial.println(F(" B:0.0 /0.0"));
            } else if (gcode.startsWith("M104")) {
                int sIndex = gcode.indexOf('S');
                if (sIndex != -1) {
                    float target = gcode.substring(sIndex + 1).toFloat();
                    if (!isnan(target)) {
                        printer.setTemp = target;
                        printer.heatDoneBeeped = false;
                        Serial.print(F("ok Set temperature to "));
                        Serial.println(printer.setTemp);
                    }
                }
            } else if (gcode.startsWith("M109")) {
                int sIndex = gcode.indexOf('S');
                if (sIndex != -1) {
                    float target = gcode.substring(sIndex + 1).toFloat();
                    if (!isnan(target)) {
                        printer.setTemp = target;
                        printer.heatDoneBeeped = false;
                        printer.waitingForHeat = true;
                        Serial.print(F("ok Heating to "));
                        Serial.println(printer.setTemp);
                    }
                }
            }
        }
        return;
    }
    gcode = getGcodeInput();
    if (gcode.length()) {
        gcode.trim();
        gcode = cleanGcode(gcode);
        strncpy(printer.currentCmd, gcode.c_str(), sizeof(printer.currentCmd) - 1);
        printer.currentCmd[sizeof(printer.currentCmd) - 1] = '\0';

        if (gcode.startsWith("G90")) {          // G90 - 進入絕對座標模式

            useAbsoluteXYZ = true;
            useRelativeE = false;
            sendOk(F("G90 Absolute mode"));
        } else if (gcode.startsWith("G91")) {   // G91 - 進入相對座標模式
            useAbsoluteXYZ = false;
            useRelativeE = true;
            sendOk(F("G91 Relative mode"));
        } else if (gcode.startsWith("M82")) {   // M82 - Extruder absolute mode
            useRelativeE = false;
            sendOk(F("M82 E absolute"));
        } else if (gcode.startsWith("M83")) {   // M83 - Extruder relative mode
            useRelativeE = true;
            sendOk(F("M83 E relative"));
        } else if (gcode.startsWith("G92")) {   // G92 - 手動設定目前座標（包含 E 也會同步進度 eStart）
            if (gcode.indexOf('X') != -1) printer.posX = gcode.substring(gcode.indexOf('X') + 1).toFloat();
            if (gcode.indexOf('Y') != -1) printer.posY = gcode.substring(gcode.indexOf('Y') + 1).toFloat();
            if (gcode.indexOf('Z') != -1) printer.posZ = gcode.substring(gcode.indexOf('Z') + 1).toFloat();
            if (gcode.indexOf('E') != -1) {
                printer.posE = gcode.substring(gcode.indexOf('E') + 1).toFloat();
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
                    Serial.print(F("ok Set temperature to "));
                    Serial.println(printer.setTemp);
                }
            }
        } else if (gcode.startsWith("M109")) {  // M109 Snnn - 設定溫度並等待
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float target = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(target)) {
                    printer.setTemp = target;
                    printer.heatDoneBeeped = false;
                    printer.waitingForHeat = true;
                    Serial.print(F("ok Heating to "));
                    Serial.println(printer.setTemp);
                }
            }
        } else if (gcode.startsWith("M105")) {  // M105 - 回報目前溫度
            Serial.print(F("ok T:"));
            Serial.print(printer.currentTemp, 1);
            Serial.print(F(" /"));
            Serial.print(printer.setTemp, 1);
            Serial.println(F(" B:0.0 /0.0"));
        } else if (gcode.startsWith("M114")) {  // M114 - 回報目前座標
            Serial.print(F("ok X:")); Serial.print(printer.posX);
            Serial.print(F(" Y:")); Serial.print(printer.posY);
            Serial.print(F(" Z:")); Serial.print(printer.posZ);
            Serial.print(F(" E:")); Serial.println(printer.posE);
        } else if (gcode.startsWith("M0")) {    // M0 - 暫停等待按鈕
            enterPauseMode();
            sendOk(F("Paused"));
        } else if (gcode.startsWith("G4")) {    // G4 Snn or Pnn - 延遲
            long ms = 0;
            int sIndex = gcode.indexOf('S');
            int pIndex = gcode.indexOf('P');
            if (sIndex != -1) {
                ms = (long)(gcode.substring(sIndex + 1).toFloat() * 1000.0);
            } else if (pIndex != -1) {
                ms = gcode.substring(pIndex + 1).toInt();
            }
            if (ms > 0) delay(ms);
            Serial.print(F("ok Dwell "));
            Serial.print(ms);
            Serial.println(F(" ms"));
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
            Serial.print(F("ok Kp:")); Serial.print(printer.Kp);
            Serial.print(F(" Ki:")); Serial.print(printer.Ki);
            Serial.print(F(" Kd:")); Serial.println(printer.Kd);
        } else if (gcode.startsWith("M400")) {  // M400 - 播放選定音樂，列印完成提示
#ifndef NO_TUNES
            playTune(DEFAULT_TUNE);
#else
            simpleBeep(buzzerPin, 1000, 200);
#endif
            sendOk(F("Print Complete"));
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
                    Serial.print(F("ok eTotal set to "));
                    Serial.println(printer.eTotal);
                }
            }
        } else if (gcode.startsWith("M220")) { // M220 Snnn - 調整移動速度倍率
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float val = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(val)) {
                    feedrateMultiplier = val / 100.0f;
                    Serial.print(F("ok Feedrate scale "));
                    Serial.print(val);
                    Serial.println(F("%"));
                }
            }
        } else if (gcode.startsWith("M221")) { // M221 Snnn - 調整擠出倍率
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float val = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(val)) {
                    flowrateMultiplier = val / 100.0f;
                    Serial.print(F("ok Flow scale "));
                    Serial.print(val);
                    Serial.println(F("%"));
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
        } else if (gcode.startsWith("M84")) {  // M84 - 馬達釋放
            digitalWrite(motorEnablePin, HIGH);
            sendOk(F("Motors disabled"));
        } else if (gcode.startsWith("G0")) {    // G0 - 快速移動，不擠料
            handleMoveCommand(gcode, false);
        } else if (gcode.startsWith("G1")) {    // G1 - 執行軸移動
            handleMoveCommand(gcode, true);
        } else if (gcode.startsWith("G28")) {   // G28 - 執行回原點並可指定軸
            bool hx = gcode.indexOf('X') != -1;
            bool hy = gcode.indexOf('Y') != -1;
            bool hz = gcode.indexOf('Z') != -1;
            if (!hx && !hy && !hz) {
                hx = hy = hz = true; // 預設全部軸
            }
            if (hx) {
                homeAxis(stepPinX, dirPinX, endstopX, "X");
                printer.posX = 0.0f;
            }
            if (hy) {
                homeAxis(stepPinY, dirPinY, endstopY, "Y");
                printer.posY = 0.0f;
            }
            if (hz) {
                homeAxis(stepPinZ, dirPinZ, endstopZ, "Z");
                printer.posZ = 0.0f;
            }
            sendOk(F("G28 Done"));
        } else {  // 其他未知指令
            Serial.print(F("ok Unknown cmd: "));
            Serial.println(gcode);
        }
    }
}


