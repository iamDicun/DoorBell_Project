#pragma once

#include <Arduino.h>

// Read PIR sensor - returns true if motion detected
bool readPIRSensor();

// Read thermistor and convert to Celsius
float readTemperatureCelsius();

// Read IR distance sensor and convert to cm
float readDistanceCm();
