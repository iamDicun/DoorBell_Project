#include "sensor_utils.h"
#include "config.h"
#include <math.h>

// Read PIR sensor for motion detection
bool readPIRSensor() {
    int pirValue = digitalRead(PIR_PIN);
    return (pirValue == HIGH); // HIGH = motion detected, LOW = no motion
}

// Read thermistor and convert to Celsius using Steinhart-Hart equation
float readTemperatureCelsius() {
    // Read ADC value (ESP32 has 12-bit ADC: 0-4095)
    int adcValue = analogRead(THERMISTOR_PIN);
    
    // Convert ADC to resistance
    // Assuming voltage divider: VCC -- THERMISTOR -- ADC_PIN -- SERIES_RESISTOR -- GND
    if (adcValue == 0) {
        return -999.0f; // Error reading
    }
    
    float voltage = adcValue * (3.3f / 4095.0f);
    float resistance = THERMISTOR_SERIES_OHMS * voltage / (3.3f - voltage);
    
    // Steinhart-Hart equation (simplified Beta parameter equation)
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float steinhart;
    steinhart = resistance / THERMISTOR_NOMINAL_OHMS;              // (R/R0)
    steinhart = log(steinhart);                                     // ln(R/R0)
    steinhart /= THERMISTOR_BETA_COEFFICIENT;                      // 1/B * ln(R/R0)
    steinhart += 1.0f / (THERMISTOR_NOMINAL_TEMP_C + 273.15f);    // + (1/T0)
    steinhart = 1.0f / steinhart;                                   // Invert
    steinhart -= 273.15f;                                           // Convert to Celsius
    
    return steinhart;
}

// Read IR distance sensor and convert to cm
float readDistanceCm() {
    // Read ADC value
    int adcValue = analogRead(IR_SENSOR_PIN);
    
    // Linear interpolation between calibrated points
    // When ADC is high (near object), distance is small
    // When ADC is low (far object), distance is large
    float distance = IR_DISTANCE_FAR_CM - 
                     ((float)(adcValue - IR_ADC_RAW_FAR) / (IR_ADC_RAW_NEAR - IR_ADC_RAW_FAR)) * 
                     (IR_DISTANCE_FAR_CM - IR_DISTANCE_NEAR_CM);
    
    // Clamp to valid range
    if (distance < IR_DISTANCE_NEAR_CM) distance = IR_DISTANCE_NEAR_CM;
    if (distance > IR_DISTANCE_FAR_CM) distance = IR_DISTANCE_FAR_CM;
    
    return distance;
}
