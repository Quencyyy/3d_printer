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

// Uncomment to enable verbose serial logging from readTemperature()
//#define DEBUG_LOGS

// Safety limits
#define MAX_HOTEND_TEMP_C 280.0f
#define HOMING_TIMEOUT_MS 15000UL

// Keep serial line reads from blocking the control loop for too long
#define SERIAL_READ_TIMEOUT_MS 5UL

// Return `error:` for unsupported commands (Marlin-like host behavior)
#define STRICT_UNKNOWN_GCODE

// PID anti-windup integral clamp
#define PID_INTEGRAL_LIMIT 300.0f
