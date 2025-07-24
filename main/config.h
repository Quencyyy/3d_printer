#pragma once

// Uncomment to feed predefined G-code without host software
// When enabled the heater output is mocked and test G-code is fed
// automatically. Motors will move according to the commands unless
// SIMULATE_EXTRUDER is also defined.
//#define SIMULATE_GCODE_INPUT

// Uncomment to bypass real heater control and simulate temperature readings
//#define SIMULATE_HEATER

// Skip real extruder movement when simulating
//#define SIMULATE_EXTRUDER
