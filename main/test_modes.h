#pragma once

// Define one of these macros to enable corresponding test mode
// #define ENABLE_BUTTON_MENU_TEST
// #define ENABLE_AXIS_CYCLE_TEST

#ifdef ENABLE_BUTTON_MENU_TEST
void testMenuSetup();
void testMenuLoop();
#endif

#ifdef ENABLE_AXIS_CYCLE_TEST
void axisTestSetup();
void axisTestLoop();
#endif

