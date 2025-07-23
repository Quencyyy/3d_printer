#include "tunes.h"
#include "pins.h"
#include "state.h"
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>

extern LiquidCrystal_I2C lcd;

// Mario tune
static const int marioNotes[] = {262, 262, 0, 262, 0, 196, 262, 0, 0, 0, 294, 0, 330};
static const int marioDur[]   = {200, 200, 100, 200, 100, 400, 400, 100, 100, 100, 400, 100, 600};

// Pachelbel's Canon simple snippet
static const int canonNotes[] = {392, 440, 494, 523, 587, 523, 494, 440, 392};
static const int canonDur[]   = {250, 250, 250, 250, 250, 250, 250, 250, 500};

// Star Wars theme snippet
static const int starNotes[]  = {440, 440, 440, 349, 523, 440, 349, 523, 440};
static const int starDur[]    = {300, 300, 300, 200, 600, 300, 200, 600, 800};

// Tetris theme snippet
static const int tetrisNotes[] = {659,494,523,587,523,494,440,440,523,659,587,523,494,523,587,659};
static const int tetrisDur[]   = {150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150};

// Simple tune for temperature reached
static const int heatNotes[] = {880, 988, 1047};
static const int heatDur[]   = {150, 150, 300};

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
        case TUNE_HEAT_DONE:
            notes = heatNotes;
            durs = heatDur;
            length = sizeof(heatNotes)/sizeof(int);
            label = "Heat";
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
