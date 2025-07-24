#pragma once
#include <Arduino.h>


// Enumeration of available completion tunes
enum TuneType {
    TUNE_MARIO = 0,
    TUNE_CANON,
    TUNE_STAR_WARS,
    TUNE_TETRIS,
    TUNE_HEAT_DONE,
    TUNE_COUNT
};

#ifndef NO_TUNES
void playTune(int tune);
#else
inline void playTune(int) {}
#endif
