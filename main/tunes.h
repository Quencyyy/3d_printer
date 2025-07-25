#pragma once
#include <Arduino.h>

// ----- Tune selection macros -----
// Define at most one of the following to select the tune included in firmware.
// If none is defined, tunes will be disabled automatically.
// Defining more than one will trigger a compile-time error.

//#define USE_TUNE_MARIO
//#define USE_TUNE_CANON
//#define USE_TUNE_STAR_WARS
//#define USE_TUNE_TETRIS

#if defined(USE_TUNE_MARIO) + defined(USE_TUNE_CANON) + defined(USE_TUNE_STAR_WARS) + defined(USE_TUNE_TETRIS) > 1
#error "Only one USE_TUNE_* macro may be defined"
#endif

#if !(defined(USE_TUNE_MARIO) || defined(USE_TUNE_CANON) || defined(USE_TUNE_STAR_WARS) || defined(USE_TUNE_TETRIS))
#define NO_TUNES
#endif

#if defined(USE_TUNE_MARIO)
#define DEFAULT_TUNE TUNE_MARIO
#elif defined(USE_TUNE_CANON)
#define DEFAULT_TUNE TUNE_CANON
#elif defined(USE_TUNE_STAR_WARS)
#define DEFAULT_TUNE TUNE_STAR_WARS
#elif defined(USE_TUNE_TETRIS)
#define DEFAULT_TUNE TUNE_TETRIS
#else
#define DEFAULT_TUNE TUNE_MARIO
#endif


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
