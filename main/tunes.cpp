#include "tunes.h"
#include "pins.h"
#include "state.h"
#include <LiquidCrystal_I2C.h>

#ifndef NO_TUNES

extern LiquidCrystal_I2C lcd;

// Completion tune selected at compile time
#if defined(USE_TUNE_MARIO)
static const int compNotes[] = {262, 262, 0, 262, 0, 196, 262, 0, 0, 0, 294, 0, 330};
static const int compDur[]   = {200, 200, 100, 200, 100, 400, 400, 100, 100, 100, 400, 100, 600};
#define COMP_LABEL "Mario"
#elif defined(USE_TUNE_CANON)
static const int compNotes[] = {392, 440, 494, 523, 587, 523, 494, 440, 392};
static const int compDur[]   = {250, 250, 250, 250, 250, 250, 250, 250, 500};
#define COMP_LABEL "Canon"
#elif defined(USE_TUNE_STAR_WARS)
static const int compNotes[]  = {440, 440, 440, 349, 523, 440, 349, 523, 440};
static const int compDur[]    = {300, 300, 300, 200, 600, 300, 200, 600, 800};
#define COMP_LABEL "StarWars"
#elif defined(USE_TUNE_TETRIS)
static const int compNotes[] = {659,494,523,587,523,494,440,440,523,659,587,523,494,523,587,659};
static const int compDur[]   = {150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150};
#define COMP_LABEL "Tetris"
#endif

// Simple tune for temperature reached
static const int heatNotes[] = {880, 988, 1047};
static const int heatDur[]   = {150, 150, 300};

void playTune(int tune) {
    const int *notes = compNotes;
    const int *durs = compDur;
    int length = sizeof(compNotes)/sizeof(int);
    const char *label = COMP_LABEL;

    if (tune == TUNE_HEAT_DONE) {
        notes = heatNotes;
        durs = heatDur;
        length = sizeof(heatNotes)/sizeof(int);
        label = "Heat";
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
        lcd.print((char)255);
    }
    noTone(buzzerPin);
    delay(500);
    lcd.clear();
}
#endif // NO_TUNES
