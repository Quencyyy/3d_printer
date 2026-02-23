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

static const size_t GCODE_BUFFER_SIZE = 96;

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

static void trimWhitespaceInPlace(char* text) {
    if (!text) return;
    size_t len = strlen(text);
    size_t start = 0;
    while (start < len && isspace((unsigned char)text[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)text[end - 1])) end--;
    if (start > 0) {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';
}

static bool getGcodeInput(char* out, size_t outLen) {
    if (!out || outLen == 0) return false;
    out[0] = '\0';
#ifdef SIMULATE_GCODE_INPUT
    if (debugIndex < debugCommandCount) {
        strncpy(out, debugCommands[debugIndex++], outLen - 1);
        out[outLen - 1] = '\0';
        Serial.print(F("DBG> "));
        Serial.println(out);
        return true;
    }
#endif
    if (!Serial.available()) return false;

    size_t readLen = Serial.readBytesUntil('\n', out, outLen - 1);
    out[readLen] = '\0';
    trimWhitespaceInPlace(out);
    return readLen > 0;
}

static bool isDigitChar(char c) {
    return c >= '0' && c <= '9';
}

static float clampTargetTemp(float target) {
    if (isnan(target)) return 0.0f;
    if (target < 0.0f) return 0.0f;
    if (target > MAX_HOTEND_TEMP_C) return MAX_HOTEND_TEMP_C;
    return target;
}

// Remove line numbers (Nxxx) and checksums (*xxx) from a raw G-code line
static void cleanGcodeInPlace(char* src) {
    if (!src) return;
    size_t srcLen = strlen(src);
    char out[GCODE_BUFFER_SIZE];
    size_t w = 0;
    for (size_t i = 0; i < srcLen && w < sizeof(out) - 1; ) {
        char c = src[i];
        if ((c == 'N' || c == 'n') && i + 1 < srcLen &&
            (isDigitChar(src[i + 1]) || src[i + 1] == '-')) {
            i++;
            while (i < srcLen && (isDigitChar(src[i]) || src[i] == '-')) i++;
            if (i < srcLen && src[i] == ' ') i++;
            continue;
        }
        if (c == '*') {
            i++;
            while (i < srcLen && isDigitChar(src[i])) i++;
            continue;
        }
        out[w++] = (char)toupper((unsigned char)c);
        i++;
    }
    out[w] = '\0';
    strncpy(src, out, GCODE_BUFFER_SIZE - 1);
    src[GCODE_BUFFER_SIZE - 1] = '\0';
    trimWhitespaceInPlace(src);
}

static bool commandIs(const char* gcode, const char* command) {
    if (!gcode || !command) return false;
    size_t n = strlen(command);
    if (strncmp(gcode, command, n) != 0) return false;
    char tail = gcode[n];
    if (tail == '\0') return true;
    if (isspace((unsigned char)tail)) return true;
    if (tail == ';') return true;
    if (isalpha((unsigned char)tail)) return true;
    return false;
}

static bool parseFloatParam(const char* gcode, char key, float& out) {
    if (!gcode) return false;
    const char* p = gcode;
    while (*p) {
        if (*p == key) {
            const char* valStart = p + 1;
            while (*valStart == ' ') valStart++;
            char* endPtr = nullptr;
            float parsed = strtof(valStart, &endPtr);
            if (endPtr != valStart) {
                out = parsed;
                return true;
            }
        }
        p++;
    }
    return false;
}

static bool parseLongParam(const char* gcode, char key, long& out) {
    if (!gcode) return false;
    const char* p = gcode;
    while (*p) {
        if (*p == key) {
            const char* valStart = p + 1;
            while (*valStart == ' ') valStart++;
            char* endPtr = nullptr;
            long parsed = strtol(valStart, &endPtr, 10);
            if (endPtr != valStart) {
                out = parsed;
                return true;
            }
        }
        p++;
    }
    return false;
}

static bool hasParam(const char* gcode, char key) {
    return gcode && strchr(gcode, key) != nullptr;
}

static void handleMoveCommand(const char* gcode, bool allowExtrude) {
    float feedrate;
    if (parseFloatParam(gcode, 'F', feedrate)) {
        int parsed = (int)lroundf(feedrate);
        if (parsed > 0) currentFeedrate = parsed;
    }

    float tx = 0, ty = 0, tz = 0, te = 0;
    bool hx = parseFloatParam(gcode, 'X', tx);
    bool hy = parseFloatParam(gcode, 'Y', ty);
    bool hz = parseFloatParam(gcode, 'Z', tz);
    bool he = allowExtrude ? parseFloatParam(gcode, 'E', te) : false;

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
        distE = useRelativeE ? te : (te - printer.posE);
        distE *= flowrateMultiplier;
    }

    float targetE = useRelativeE ? distE : (printer.posE + distE);

    printer.remainingStepsX = lroundf(fabsf(distX * stepsPerMM_X));
    printer.remainingStepsY = lroundf(fabsf(distY * stepsPerMM_Y));
    printer.remainingStepsZ = lroundf(fabsf(distZ * stepsPerMM_Z));
    printer.remainingStepsE = lroundf(fabsf(distE * stepsPerMM_E));
    printer.signX = (distX >= 0) ? 1 : -1;
    printer.signY = (distY >= 0) ? 1 : -1;
    printer.signZ = (distZ >= 0) ? 1 : -1;
    printer.signE = (distE >= 0) ? 1 : -1;

    printer.nextX = useAbsoluteXYZ ? tx : distX;
    printer.nextY = useAbsoluteXYZ ? ty : distY;
    printer.nextZ = useAbsoluteXYZ ? tz : distZ;
    if (allowExtrude) {
        printer.nextE = useRelativeE ? distE : targetE;
    } else {
        printer.nextE = useRelativeE ? 0 : printer.posE;
    }
    printer.hasNextMove = true;

    moveAxes(tx, ty, tz, targetE, lroundf(currentFeedrate * feedrateMultiplier));

    printer.hasNextMove = false;
    printer.remainingStepsX = printer.remainingStepsY = printer.remainingStepsZ = printer.remainingStepsE = 0;

    Serial.print(F("ok Move"));
    if (hx) { Serial.print(F(" X")); Serial.print(printer.posX); }
    if (hy) { Serial.print(F(" Y")); Serial.print(printer.posY); }
    if (hz) { Serial.print(F(" Z")); Serial.print(printer.posZ); }
    if (allowExtrude && (he || distE != 0)) { Serial.print(F(" E")); Serial.print(printer.posE); }
    Serial.println();
}

void processGcode() {
    char gcode[GCODE_BUFFER_SIZE] = {0};
    if (printer.waitingForHeat) {
        if (fabs(printer.currentTemp - printer.setTemp) < 1.0 && printer.heatDoneBeeped) {
            printer.waitingForHeat = false;
            sendOk(F("Target temp reached"));
        }
        if (getGcodeInput(gcode, sizeof(gcode))) {
            cleanGcodeInPlace(gcode);
            if (commandIs(gcode, "M105")) {
                Serial.print(F("ok T:"));
                Serial.print(printer.currentTemp, 1);
                Serial.print(F(" /"));
                Serial.print(printer.setTemp, 1);
                Serial.println(F(" B:0.0 /0.0"));
            } else if (commandIs(gcode, "M104")) {
                float target;
                if (parseFloatParam(gcode, 'S', target)) {
                    printer.setTemp = clampTargetTemp(target);
                    printer.heatDoneBeeped = false;
                    Serial.print(F("ok Set temperature to "));
                    Serial.println(printer.setTemp);
                }
            } else if (commandIs(gcode, "M109")) {
                float target;
                if (parseFloatParam(gcode, 'S', target)) {
                    printer.setTemp = clampTargetTemp(target);
                    printer.heatDoneBeeped = false;
                    printer.waitingForHeat = true;
                    Serial.print(F("ok Heating to "));
                    Serial.println(printer.setTemp);
                }
            }
        }
        return;
    }

    if (printer.dwellActive) {
        if ((long)(millis() - printer.dwellUntil) >= 0) {
            printer.dwellActive = false;
            sendOk(F("Dwell done"));
        }
        return;
    }

    if (getGcodeInput(gcode, sizeof(gcode))) {
        cleanGcodeInPlace(gcode);
        strncpy(printer.currentCmd, gcode, sizeof(printer.currentCmd) - 1);
        printer.currentCmd[sizeof(printer.currentCmd) - 1] = '\0';

        if (commandIs(gcode, "G90")) {          // G90 - 進入絕對座標模式 (XYZ only)

            useAbsoluteXYZ = true;
            sendOk(F("G90 XYZ absolute"));
        } else if (commandIs(gcode, "G91")) {   // G91 - 進入相對座標模式 (XYZ only)
            useAbsoluteXYZ = false;
            sendOk(F("G91 XYZ relative"));
        } else if (commandIs(gcode, "M82")) {   // M82 - Extruder absolute mode
            useRelativeE = false;
            sendOk(F("M82 E absolute"));
        } else if (commandIs(gcode, "M83")) {   // M83 - Extruder relative mode
            useRelativeE = true;
            sendOk(F("M83 E relative"));
        } else if (commandIs(gcode, "G92")) {   // G92 - 手動設定目前座標（包含 E 也會同步進度起點）
            float value;
            if (parseFloatParam(gcode, 'X', value)) printer.posX = value;
            if (parseFloatParam(gcode, 'Y', value)) printer.posY = value;
            if (parseFloatParam(gcode, 'Z', value)) printer.posZ = value;
            if (parseFloatParam(gcode, 'E', value)) {
                printer.posE = value;
                printer.extrusionStartMM = printer.posE;  // 同步進度起點，避免重設座標後估算錯誤
                sendOk(F("G92 E origin reset"));
            } else {
                sendOk(F("G92 Origin set"));
            }
        } else if (commandIs(gcode, "M104")) {  // M104 Snnn - 設定加熱目標溫度（不等待）
            float target;
            if (parseFloatParam(gcode, 'S', target)) {
                printer.setTemp = clampTargetTemp(target);
                printer.heatDoneBeeped = false;
                Serial.print(F("ok Set temperature to "));
                Serial.println(printer.setTemp);
            }
        } else if (commandIs(gcode, "M109")) {  // M109 Snnn - 設定溫度並等待
            float target;
            if (parseFloatParam(gcode, 'S', target)) {
                printer.setTemp = clampTargetTemp(target);
                printer.heatDoneBeeped = false;
                printer.waitingForHeat = true;
                Serial.print(F("ok Heating to "));
                Serial.println(printer.setTemp);
            }
        } else if (commandIs(gcode, "M105")) {  // M105 - 回報目前溫度
            Serial.print(F("ok T:"));
            Serial.print(printer.currentTemp, 1);
            Serial.print(F(" /"));
            Serial.print(printer.setTemp, 1);
            Serial.println(F(" B:0.0 /0.0"));
        } else if (commandIs(gcode, "M114")) {  // M114 - 回報目前座標
            Serial.print(F("ok X:")); Serial.print(printer.posX);
            Serial.print(F(" Y:")); Serial.print(printer.posY);
            Serial.print(F(" Z:")); Serial.print(printer.posZ);
            Serial.print(F(" E:")); Serial.println(printer.posE);
        } else if (commandIs(gcode, "M0")) {    // M0 - 暫停等待按鈕
            enterPauseMode();
            printer.dwellActive = false;
            sendOk(F("Paused"));
        } else if (commandIs(gcode, "G4")) {    // G4 Snn or Pnn - 延遲
            long ms = 0;
            float sec;
            long pMs;
            if (parseFloatParam(gcode, 'S', sec)) {
                ms = (long)(sec * 1000.0f);
            } else if (parseLongParam(gcode, 'P', pMs)) {
                ms = pMs;
            }
            if (ms <= 0) {
                sendOk(F("Dwell 0 ms"));
            } else {
                printer.dwellUntil = millis() + (unsigned long)ms;
                printer.dwellActive = true;
                Serial.print(F("ok Dwell "));
                Serial.print(ms);
                Serial.println(F(" ms"));
            }
        } else if (commandIs(gcode, "M301")) {  // M301 Pn In Dn - 設定 PID 控制參數
            float temp;
            if (parseFloatParam(gcode, 'P', temp)) printer.Kp = temp;
            if (parseFloatParam(gcode, 'I', temp)) printer.Ki = temp;
            if (parseFloatParam(gcode, 'D', temp)) printer.Kd = temp;

            saveSettingsToEEPROM();
            Serial.print(F("ok Kp:")); Serial.print(printer.Kp);
            Serial.print(F(" Ki:")); Serial.print(printer.Ki);
            Serial.print(F(" Kd:")); Serial.println(printer.Kd);
        } else if (commandIs(gcode, "M400")) {  // M400 - 播放選定音樂，列印完成提示
#ifndef NO_TUNES
            playTune(DEFAULT_TUNE);
#else
            simpleBeep(buzzerPin, 1000, 200);
#endif
            sendOk(F("Print Complete"));
        } else if (commandIs(gcode, "M92")) {   // M92 - 設定各軸 steps/mm
            float val;
            if (parseFloatParam(gcode, 'X', val)) stepsPerMM_X = val;
            if (parseFloatParam(gcode, 'Y', val)) stepsPerMM_Y = val;
            if (parseFloatParam(gcode, 'Z', val)) stepsPerMM_Z = val;
            if (parseFloatParam(gcode, 'E', val)) stepsPerMM_E = val;
            sendOk(F("Steps per mm updated"));
        } else if (commandIs(gcode, "M290")) { // M290 En - 設定進度總量
            long val;
            if (parseLongParam(gcode, 'E', val) && val > 0) {
                printer.extrusionTotalMM = val;
                printer.extrusionStartMM = printer.posE;
                printer.isExtrusionStartSynced = true;
                printer.progress = 0;
                Serial.print(F("ok extrusionTotalMM set to "));
                Serial.println(printer.extrusionTotalMM);
            }
        } else if (commandIs(gcode, "M220")) { // M220 Snnn - 調整移動速度倍率
            float val;
            if (parseFloatParam(gcode, 'S', val)) {
                feedrateMultiplier = val / 100.0f;
                Serial.print(F("ok Feedrate scale "));
                Serial.print(val);
                Serial.println(F("%"));
            }
        } else if (commandIs(gcode, "M221")) { // M221 Snnn - 調整擠出倍率
            float val;
            if (parseFloatParam(gcode, 'S', val)) {
                flowrateMultiplier = val / 100.0f;
                Serial.print(F("ok Flow scale "));
                Serial.print(val);
                Serial.println(F("%"));
            }
        } else if (commandIs(gcode, "M500")) {  // M500 - 儲存設定到 EEPROM
            saveSettingsToEEPROM();
            sendOk(F("Settings saved"));
        } else if (commandIs(gcode, "M503")) {  // M503 - 印出目前參數
            sendOk(F("Current settings"));
            Serial.print(F("Kp = ")); Serial.println(printer.Kp);
            Serial.print(F("Ki = ")); Serial.println(printer.Ki);
            Serial.print(F("Kd = ")); Serial.println(printer.Kd);
            Serial.print(F("Steps/mm X:")); Serial.println(stepsPerMM_X);
            Serial.print(F("Steps/mm Y:")); Serial.println(stepsPerMM_Y);
            Serial.print(F("Steps/mm Z:")); Serial.println(stepsPerMM_Z);
            Serial.print(F("Steps/mm E:")); Serial.println(stepsPerMM_E);
        } else if (commandIs(gcode, "M84")) {  // M84 - 馬達釋放
            digitalWrite(motorEnablePin, HIGH);
            sendOk(F("Motors disabled"));
        } else if (commandIs(gcode, "G0")) {    // G0 - 快速移動，不擠料
            handleMoveCommand(gcode, false);
        } else if (commandIs(gcode, "G1")) {    // G1 - 執行軸移動
            handleMoveCommand(gcode, true);
        } else if (commandIs(gcode, "G28")) {   // G28 - 執行回原點並可指定軸
            bool hx = hasParam(gcode, 'X');
            bool hy = hasParam(gcode, 'Y');
            bool hz = hasParam(gcode, 'Z');
            bool ok = true;
            if (!hx && !hy && !hz) {
                hx = hy = hz = true; // 預設全部軸
            }
            if (hx) {
                if (homeAxis(stepPinX, dirPinX, endstopX, "X", HOMING_TIMEOUT_MS)) {
                    printer.posX = 0.0f;
                } else {
                    ok = false;
                }
            }
            if (hy) {
                if (homeAxis(stepPinY, dirPinY, endstopY, "Y", HOMING_TIMEOUT_MS)) {
                    printer.posY = 0.0f;
                } else {
                    ok = false;
                }
            }
            if (hz) {
                if (homeAxis(stepPinZ, dirPinZ, endstopZ, "Z", HOMING_TIMEOUT_MS)) {
                    printer.posZ = 0.0f;
                } else {
                    ok = false;
                }
            }
            if (ok) {
                sendOk(F("G28 Done"));
            } else {
                Serial.println(F("error: G28 failed"));
            }
        } else {  // 其他未知指令
#ifdef STRICT_UNKNOWN_GCODE
            Serial.print(F("error: Unknown cmd: "));
            Serial.println(gcode);
#else
            Serial.print(F("ok Unknown cmd: "));
            Serial.println(gcode);
#endif
        }
    }
}


