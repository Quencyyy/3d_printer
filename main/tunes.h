#pragma once
#include <Arduino.h>

// Uncomment to enable buzzer features
// #define ENABLE_BUZZER

// Enumeration of available completion tunes
enum TuneType {
    TUNE_MARIO = 0,
    TUNE_CANON,
    TUNE_STAR_WARS,
    TUNE_TETRIS,
    TUNE_HEAT_DONE,
    TUNE_COUNT
};

#ifdef ENABLE_BUZZER
void playTune(int tune);
#else
inline void playTune(int tune) {}
#endif
