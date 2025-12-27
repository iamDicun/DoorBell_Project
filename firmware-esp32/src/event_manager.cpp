#include "event_manager.h"

#include <Arduino.h>
#include "config.h"
#include "audio_service.h"
#include "camera_service.h"
#include "mqtt_service.h"

static bool lastButtonState = HIGH;
static bool stableButtonState = HIGH;
static unsigned long lastDebounceTime = 0;
static unsigned long buttonPressedTime = 0;
static bool longPressTriggered = false;
static bool pirState = false;
static unsigned long lastTelemetryTime = 0;

// Debug counters
static unsigned long buttonReadCount = 0;
static unsigned long debounceRejectCount = 0;
static unsigned long stateChangeCount = 0;

std::queue<Event> eventQueue;
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
    pinMode(BUTTON_PIN, INPUT);  // Changed from INPUT_PULLUP
    pinMode(PIR_PIN, INPUT);
    pinMode(THERMISTOR_PIN, INPUT);
    lastButtonState = digitalRead(BUTTON_PIN);
    stableButtonState = lastButtonState;
    longPressTriggered = false;
    
    Serial.println("\n========================================");
    Serial.println("[EVT] Event Manager Initialized");
    Serial.printf("[EVT] BUTTON_PIN: GPIO %d (INPUT mode)\n", BUTTON_PIN);
    Serial.printf("[EVT] Initial button state: %s\n", lastButtonState == HIGH ? "HIGH (PRESSED)" : "LOW (not pressed)");
    Serial.printf("[EVT] PIR_PIN: GPIO %d (INPUT mode)\n", PIR_PIN);
    Serial.printf("[EVT] Initial PIR state: %d\n", digitalRead(PIR_PIN));
    Serial.printf("[EVT] DEBOUNCE_MS: %d ms\n", DEBOUNCE_MS);
    Serial.printf("[EVT] SHORT_PRESS: %d ms\n", BUTTON_SHORT_PRESS_MS);
    Serial.printf("[EVT] LONG_PRESS: %d ms\n", BUTTON_LONG_PRESS_MS);
    Serial.println("========================================\n");
}

void eventManagerLoop() {
    unsigned long now = millis();
    bool currentState = digitalRead(BUTTON_PIN);
    buttonReadCount++;
    
    // Debug: Print every 1000 reads (approximately every 5 seconds)
    if (buttonReadCount % 1000 == 0) {
        Serial.printf("[EVT-DEBUG] Total reads: %lu | State changes: %lu | Debounce rejects: %lu | Current: %s | Stable: %s\n",
                     buttonReadCount, stateChangeCount, debounceRejectCount,
                     currentState == HIGH ? "HIGH" : "LOW",
                     stableButtonState == HIGH ? "HIGH" : "LOW");
    }

    // Debounce check
    if (currentState != lastButtonState) {
        lastDebounceTime = now;
        Serial.printf("[EVT-RAW] Button raw state changed: %s -> %s at %lu ms\n",
                     lastButtonState == HIGH ? "HIGH" : "LOW",
                     currentState == HIGH ? "HIGH" : "LOW",
                     now);
    }

    if ((now - lastDebounceTime) > DEBOUNCE_MS) {
        if (currentState != stableButtonState) {
            stableButtonState = currentState;
            stateChangeCount++;
            
            Serial.println("\n========== BUTTON EVENT ==========");
            Serial.printf("[EVT] Stable state changed to: %s\n", stableButtonState == HIGH ? "HIGH" : "LOW");
            Serial.printf("[EVT] Time since last debounce: %lu ms\n", now - lastDebounceTime);

            if (stableButtonState == HIGH) {  // Changed: HIGH = pressed
                // Button just pressed
                buttonPressedTime = now;
                longPressTriggered = false;
                
                Serial.println(">>> BUTTON PRESSED <<<");
                Serial.printf("[EVT] Press started at: %lu ms\n", buttonPressedTime);
                Serial.println("==================================\n");
            } else {  // Changed: LOW = released
                // Button just released
                unsigned long held = now - buttonPressedTime;
                
                Serial.println(">>> BUTTON RELEASED <<<");
                Serial.printf("[EVT] Press duration: %lu ms\n", held);
                
                if (longPressTriggered) {
                    Serial.println("[EVT] → LONG PRESS RELEASE EVENT");
                    eventQueue.push({EVENT_BUTTON_LONG_RELEASE, now});
                } else {
                    if (held >= BUTTON_SHORT_PRESS_MS) {
                        Serial.println("[EVT] → SHORT PRESS EVENT");
                        eventQueue.push({EVENT_BUTTON_SHORT_PRESS, now});
                    } else {
                        Serial.printf("[EVT] → IGNORED (too short: %lu ms < %d ms)\n", held, BUTTON_SHORT_PRESS_MS);
                    }
                }
                Serial.println("==================================\n");
            }
        }
    } else {
        // In debounce period - count rejections
        if (currentState != lastButtonState) {
            debounceRejectCount++;
            if (debounceRejectCount % 10 == 0) {
                Serial.printf("[EVT-DEBOUNCE] Rejected %lu state changes (in debounce period)\n", debounceRejectCount);
            }
        }
    }

    // Check for long press while button is held
    if (stableButtonState == HIGH && !longPressTriggered) {  // Changed: HIGH = pressed
        unsigned long held = now - buttonPressedTime;
        if (held >= BUTTON_LONG_PRESS_MS) {
            Serial.println("\n========== LONG PRESS DETECTED ==========");
            Serial.printf("[EVT] Button held for: %lu ms (threshold: %d ms)\n", held, BUTTON_LONG_PRESS_MS);
            Serial.println(">>> LONG PRESS EVENT <<<");
            Serial.println("=========================================\n");
            
            longPressTriggered = true;
            eventQueue.push({EVENT_BUTTON_LONG_PRESS, now});
        }
    }

    lastButtonState = currentState;

    // PIR motion detection - state tracking only (alert handling in doorbell_app.cpp)
    int pirRawValue = digitalRead(PIR_PIN);
    bool pir = (pirRawValue == HIGH);
    
    // Debug: Print raw PIR value every 1000 reads
    static unsigned long pirReadCount = 0;
    pirReadCount++;
    if (pirReadCount % 1000 == 0) {
        Serial.printf("[PIR-DEBUG] Read#%lu | PIN=%d | Raw=%d | State=%s\n",
                     pirReadCount, PIR_PIN, pirRawValue, pirState ? "HIGH" : "LOW");
    }
    
    if (pir && !pirState) {
        pirState = true;
        Serial.printf("\n[EVT-PIR] *** MOTION DETECTED *** (Raw pin value: %d)\n", pirRawValue);
        // Register detection for alert level aggregation
        extern void registerPIRDetection(unsigned long timestamp);
        registerPIRDetection(millis());
    } else if (!pir && pirState) {
        pirState = false;
        Serial.printf("[EVT-PIR] Motion cleared (Raw pin value: %d)\n\n", pirRawValue);
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
        
        Serial.printf("\n[EVT-TELEMETRY] Temp: %.2f°C, Motion: %s\n\n", 
                     temperature, 
                     pirState ? "YES" : "NO");
        
        mqttPublishJson(MQTT_TOPIC_TELEMETRY, telemetryMsg);
    }
}

Event eventPop() {
    if (eventQueue.empty()) {
        return {EVENT_NONE, 0};
    }
    Event evt = eventQueue.front();
    eventQueue.pop();
    return evt;
}

bool eventAvailable() {
    return !eventQueue.empty();
}
