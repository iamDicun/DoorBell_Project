#include "event_manager.h"

#include <Arduino.h>
#include "config.h"
#include "audio_service.h"
#include "camera_service.h"
#include "mqtt_service.h"

static bool buttonState = false;
static unsigned long buttonPressedAt = 0;
static bool pirState = false;
static unsigned long lastTelemetryTime = 0;

// Helper function to read temperature from thermistor
static float readTemperatureCelsius() {
    int adcValue = analogRead(THERMISTOR_PIN);
    
    // Convert ADC to voltage (ESP32-S3 ADC is 12-bit: 0-4095)
    float voltage = (adcValue / 4095.0) * 3.3;
    
    // Calculate resistance of thermistor
    float resistance = THERMISTOR_SERIES_OHMS * voltage / (3.3 - voltage);
    
    // Steinhart-Hart equation (simplified Beta parameter equation)
    float steinhart;
    steinhart = resistance / THERMISTOR_NOMINAL_OHMS;                   // (R/Ro)
    steinhart = log(steinhart);                                         // ln(R/Ro)
    steinhart /= THERMISTOR_BETA_COEFFICIENT;                           // 1/B * ln(R/Ro)
    steinhart += 1.0 / (THERMISTOR_NOMINAL_TEMP_C + 273.15);           // + (1/To)
    steinhart = 1.0 / steinhart;                                        // Invert
    steinhart -= 273.15;                                                // convert to Celsius
    
    return steinhart;
}

void eventManagerInit() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(PIR_PIN, INPUT);
    pinMode(THERMISTOR_PIN, INPUT);
    Serial.println("[EVT] manager init");
}

void eventManagerLoop() {
    // Button handling
    bool btn = digitalRead(BUTTON_PIN) == LOW;
    if (btn && !buttonState) {
        buttonPressedAt = millis();
        buttonState = true;
    } else if (!btn && buttonState) {
        unsigned long held = millis() - buttonPressedAt;
        buttonState = false;
        if (held >= BUTTON_LONG_MS) {
            Serial.println("[EVT] long press -> record voice note");
            audioServiceStartVoiceNote();
        } else {
            Serial.println("[EVT] short press -> play chime + MQTT ring");
            audioServicePlayChime();
            mqttPublishJson(MQTT_TOPIC_STATUS, "{\"event\":\"ring\"}");
        }
    }

    // PIR motion detection - simplified to presence detection
    bool pir = digitalRead(PIR_PIN);
    if (pir && !pirState) {
        pirState = true;
        Serial.println("[EVT] motion detected - presence: YES");
        mqttPublishJson(MQTT_TOPIC_SECURITY, "{\"motion\":true,\"alert\":\"motion_detected\"}");
        cameraServiceCaptureBurst();
    } else if (!pir && pirState) {
        pirState = false;
        Serial.println("[EVT] motion cleared - presence: NO");
        mqttPublishJson(MQTT_TOPIC_SECURITY, "{\"motion\":false}");
    }
    
    // Send telemetry data every 60 seconds
    if (millis() - lastTelemetryTime > 60000) {
        lastTelemetryTime = millis();
        
        float temperature = readTemperatureCelsius();
        
        // Create telemetry JSON
        char telemetryMsg[128];
        snprintf(telemetryMsg, sizeof(telemetryMsg), 
                 "{\"temperature\":%.2f,\"motion\":%s}", 
                 temperature, 
                 pirState ? "true" : "false");
        
        Serial.printf("[EVT] Telemetry - Temp: %.2f°C, Motion: %s\n", 
                     temperature, 
                     pirState ? "YES" : "NO");
        
        mqttPublishJson(MQTT_TOPIC_TELEMETRY, telemetryMsg);
    }
}
