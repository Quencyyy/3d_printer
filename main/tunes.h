#pragma once
#include <Arduino.h>

// Enumeration of available completion tunes
enum TuneType {
    TUNE_MARIO = 0,
    TUNE_CANON,
    TUNE_STAR_WARS,
    TUNE_TETRIS,
    TUNE_COUNT
};

extern int currentTune;

void playTune(int tune);
