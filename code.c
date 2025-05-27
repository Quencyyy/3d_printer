//#define ENABLE_HOMING

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <Bounce2.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Bounce debouncer = Bounce();

const int tempPin = A0;
const int heaterPin = 9;
const int fanPin = 10;
const int buzzerPin = 8;
const int buttonPin = 11;

const int stepPinX = 2, dirPinX = 5, endstopX = A1;
const int stepPinY = 3, dirPinY = 6, endstopY = A2;
const int stepPinZ = 4, dirPinZ = 7, endstopZ = A3;
const int stepPinE = 12, dirPinE = 13;

float setTemp = 0.0;
float Kp = 20, Ki = 1, Kd = 50;
float integral = 0, previousError = 0;
unsigned long lastTime = 0;

bool fanStarted = false;
bool fanForced = false;
bool heatDoneBeeped = false;
float currentTemp = 0.0;
unsigned long heatStableStart = 0;
const unsigned long stableHoldTime = 3000;

long posX = 0, posY = 0, posZ = 0, posE = 0;
bool useAbsolute = true;

int displayMode = 0;
unsigned long lastPressTime = 0;
bool isLongPress = false;
bool confirmStop = false;
unsigned long confirmStartTime = 0;

unsigned long lastLoopTime = 0;
const unsigned long loopInterval = 100;

void saveSettingsToEEPROM() {
  EEPROM.put(0, Kp);
  EEPROM.put(4, Ki);
  EEPROM.put(8, Kd);
  EEPROM.put(12, setTemp);
}

void loadSettingsFromEEPROM() {
  EEPROM.get(0, Kp);
  EEPROM.get(4, Ki);
  EEPROM.get(8, Kd);
  EEPROM.get(12, setTemp);
}

void readTemperature() {
  int raw = analogRead(tempPin);
  float voltage = raw * 5.0 / 1023.0;
  float resistance = (5.0 - voltage) * 10000.0 / voltage;
  currentTemp = 1.0 / (log(resistance / 10000.0) / 3950.0 + 1.0 / 298.15) - 273.15;
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

    if (currentTemp >= 50 && !fanStarted && !fanForced) {
      digitalWrite(fanPin, HIGH);
      fanStarted = true;
    }

    if (abs(currentTemp - setTemp) < 1.0) {
      if (!heatDoneBeeped && heatStableStart == 0) {
        heatStableStart = now;
      }
      if (!heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
        tone(buzzerPin, 1000, 200); // 加熱完成簡單提示音
        heatDoneBeeped = true;
      }
    } else {
      heatStableStart = 0;
    }
  } else {
    analogWrite(heaterPin, 0);
    if (!fanForced) {
      digitalWrite(fanPin, LOW);
      fanStarted = false;
    }
    heatDoneBeeped = false;
    heatStableStart = 0;
  }
}

void updateLCD() {
  static int animPos = 0;
  static const char anim[] = "|/-\\";

  lcd.setCursor(0, 0);
  if (displayMode == 0) {
    lcd.print("T:");
    lcd.print(currentTemp, 1);
    lcd.print((char)223);
    lcd.print("C Set:");
    lcd.print(setTemp, 0);
  } else if (displayMode == 1) {
    lcd.print("X"); lcd.print(posX);
    lcd.print(" Y"); lcd.print(posY);
    lcd.setCursor(0, 1);
    lcd.print("Z"); lcd.print(posZ);
    lcd.print(" E"); lcd.print(posE);
    return;
  } else {
    lcd.print("Status:");
    lcd.print(heatDoneBeeped ? " ✓ Ready   " : " Heating...");
  }

  lcd.setCursor(15, 1);
  lcd.print(anim[animPos]);
  animPos = (animPos + 1) % 4;
} 

void checkButton() {
  debouncer.update();
  bool state = debouncer.read() == LOW;

  if (state && !isLongPress && millis() - lastPressTime > 50) {
    if (millis() - lastPressTime > 3000) {
      if (confirmStop) {
        if (millis() - confirmStartTime >= 3000) {
          forceStop();
          confirmStop = false;
        }
      } else {
        confirmStop = true;
        confirmStartTime = millis();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Confirm Stop?");
        lcd.setCursor(0, 1);
        lcd.print("Hold 3s again");
      }
      isLongPress = true;
    }
  }

  if (!state) {
    if (confirmStop && millis() - confirmStartTime < 5000) {
      confirmStop = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cancelled");
      delay(300);
    } else if (!isLongPress) {
      displayMode = (displayMode + 1) % 3;
    }
    isLongPress = false;
    lastPressTime = millis();
  }
}

void forceStop() {
  setTemp = 0;
  analogWrite(heaterPin, 0);
  digitalWrite(fanPin, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("** Forced STOP **");
}

void playMario() {
  int melody[] = {262, 262, 0, 262, 0, 196, 262, 0, 0, 0, 294, 0, 330};
  int durations[] = {200, 200, 100, 200, 100, 400, 400, 100, 100, 100, 400, 100, 600};

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tune: Mario");
  lcd.setCursor(0, 1);

  for (int i = 0; i < 13; i++) {
    if (melody[i] == 0) {
      noTone(buzzerPin);
    } else {
      tone(buzzerPin, melody[i], durations[i]);
    }
    delay(durations[i] + 50);
    lcd.print((char)255); // 進度條
  }
  noTone(buzzerPin);
  delay(500);
  lcd.clear();
}

void moveAxis(int stepPin, int dirPin, long& pos, int target) {
  int moveSteps = useAbsolute ? target - pos : target;
  bool direction = moveSteps >= 0;

  if (&pos == &posE) {
    const long eMaxSteps = 5000;
    long newPos = pos + moveSteps;
    if (abs(newPos) > eMaxSteps) {
      Serial.println("E-axis limit exceeded!");
      return;
    }
  }

  digitalWrite(dirPin, direction ? HIGH : LOW);
  for (int i = 0; i < abs(moveSteps); i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(800);
  }
  pos += moveSteps;
}

#ifdef ENABLE_HOMING
void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label) {
  digitalWrite(dirPin, LOW);
  while (digitalRead(endstopPin) == HIGH) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(800);
  }
  Serial.print(label); Serial.println(" Homed");
}
#endif

void processGcode() {
  if (Serial.available()) {
    String gcode = Serial.readStringUntil('\n');
    gcode.trim();

    if (gcode.startsWith("G90")) {
      useAbsolute = true;
      Serial.println("[G90] Absolute mode");
    } else if (gcode.startsWith("G91")) {
      useAbsolute = false;
      Serial.println("[G91] Relative mode");
    } else if (gcode.startsWith("G92")) {
      if (gcode.indexOf('X') != -1) posX = gcode.substring(gcode.indexOf('X') + 1).toInt();
      if (gcode.indexOf('Y') != -1) posY = gcode.substring(gcode.indexOf('Y') + 1).toInt();
      if (gcode.indexOf('Z') != -1) posZ = gcode.substring(gcode.indexOf('Z') + 1).toInt();
      if (gcode.indexOf('E') != -1) posE = gcode.substring(gcode.indexOf('E') + 1).toInt();
      Serial.println("[G92] Origin set.");
    } else if (gcode.startsWith("M104")) {
      int sIndex = gcode.indexOf('S');
      if (sIndex != -1) {
        float target = gcode.substring(sIndex + 1).toFloat();
        if (!isnan(target)) {
          setTemp = target;
          heatDoneBeeped = false;
          Serial.print("Set temperature to ");
          Serial.println(setTemp);
        }
      }
    } else if (gcode.startsWith("M105")) {
      Serial.print("T:");
      Serial.println(currentTemp);
    } else if (gcode.startsWith("M106")) {
      fanForced = true;
      digitalWrite(fanPin, HIGH);
      Serial.println("Fan ON");
    } else if (gcode.startsWith("M107")) {
      fanForced = false;
      digitalWrite(fanPin, LOW);
      fanStarted = false;
      Serial.println("Fan OFF");
    } else if (gcode.startsWith("M301")) {
      int pIndex = gcode.indexOf('P');
      int iIndex = gcode.indexOf('I');
      int dIndex = gcode.indexOf('D');

      float temp;
      if (pIndex != -1) {
        temp = gcode.substring(pIndex + 1, (iIndex != -1 ? iIndex : gcode.length())).toFloat();
        if (!isnan(temp)) Kp = temp;
      }
      if (iIndex != -1) {
        temp = gcode.substring(iIndex + 1, (dIndex != -1 ? dIndex : gcode.length())).toFloat();
        if (!isnan(temp)) Ki = temp;
      }
      if (dIndex != -1) {
        temp = gcode.substring(dIndex + 1).toFloat();
        if (!isnan(temp)) Kd = temp;
      }

      saveSettingsToEEPROM();
      Serial.println("[M301] PID updated:");
      Serial.print("Kp = "); Serial.println(Kp);
      Serial.print("Ki = "); Serial.println(Ki);
      Serial.print("Kd = "); Serial.println(Kd);
    } 
      else if (gcode.startsWith("M400")) { 
      playMario(); 
      Serial.println("[M400] Print Complete"); 
    }
      else if (gcode.startsWith("G1")) {
      handleG1Axis('X', stepPinX, dirPinX, posX, gcode);
      handleG1Axis('Y', stepPinY, dirPinY, posY, gcode);
      handleG1Axis('Z', stepPinZ, dirPinZ, posZ, gcode);
      handleG1Axis('E', stepPinE, dirPinE, posE, gcode);
    } else if (gcode.startsWith("G28")) {
#ifdef ENABLE_HOMING
      homeAxis(stepPinX, dirPinX, endstopX, "X");
      homeAxis(stepPinY, dirPinY, endstopY, "Y");
      homeAxis(stepPinZ, dirPinZ, endstopZ, "Z");
#else
      Serial.println("[Homing disabled] Please home manually.");
#endif
    } else {
      Serial.print("Unknown command: [");
      Serial.print(gcode);
      Serial.println("]");
    }
  }
}

void handleG1Axis(char axis, int stepPin, int dirPin, long& pos, String& gcode) {
  int idx = gcode.indexOf(axis);
  if (idx != -1) {
    int end = gcode.indexOf(' ', idx);
    String valStr = (end != -1) ? gcode.substring(idx + 1, end) : gcode.substring(idx + 1);
    int val = valStr.toInt();
    moveAxis(stepPin, dirPin, pos, val);
    Serial.print("Move "); Serial.print(axis); Serial.print(" to ");
    Serial.println(useAbsolute ? val : pos);
  }
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  debouncer.attach(buttonPin);
  debouncer.interval(25);

  pinMode(heaterPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();

  Serial.begin(9600);
  loadSettingsFromEEPROM();
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime >= loopInterval) {
    lastLoopTime = now;
    readTemperature();
    controlHeater();
    checkButton();
    updateLCD();
    processGcode();
  }
}

