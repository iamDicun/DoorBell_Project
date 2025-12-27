#include "doorbell_app.h"

#include <Arduino.h>
#include "config.h"
#include "audio.h"
#include "camera_module.h"
#include "mqtt_service.h"
#include "event_manager.h"
#include "audio_service.h"
#include "camera_service.h"
#include "sensor_utils.h"
#include "doorbell_features.h"
#include <SPIFFS.h>

// Sensor reading intervals
static unsigned long lastTempRead = 0;
static unsigned long loopCounter = 0;

#define TELEMETRY_PUBLISH_INTERVAL 10000 // Publish full telemetry every 10s

static void printBanner() {
    Serial.println();
    Serial.println("=== ESP32-S3 Smart Doorbell ===");
    Serial.println("Features: Ding-Dong, Voice Notes, Security Cam, PIR Alert");
}

void doorbellSetup() {
    printBanner();

    // Initialize SPIFFS for audio files
    if (!SPIFFS.begin(true)) {
        Serial.println("[ERR] SPIFFS init failed");
    } else {
        Serial.println("[SPIFFS] Initialized");
    }

    // Initialize sensor pins
    pinMode(BUTTON_PIN, INPUT);  // Changed from INPUT_PULLUP - button has external pullup
    pinMode(PIR_PIN, INPUT);
    pinMode(THERMISTOR_PIN, INPUT);
    pinMode(IR_SENSOR_PIN, INPUT);
    Serial.println("[Sensors] Pins initialized");

    if (!initCamera()) {
        Serial.println("[ERR] Camera init failed");
    }
    if (!initMicrophone()) {
        Serial.println("[ERR] Microphone init failed");
    }
    if (!initSpeaker()) {
        Serial.println("[ERR] Speaker init failed");
    }

    mqttServiceInit();
    eventManagerInit();
    audioServiceInit();
    cameraServiceInit();

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   BUTTON TEST MODE - MQTT DISABLED     ║");
    Serial.println("║   Press button to see events           ║");
    Serial.println("╚════════════════════════════════════════╝\n");
}

void doorbellLoop() {
    loopCounter++;
    
    // Print loop status every 5000 iterations (approximately every 25 seconds)
    if (loopCounter % 5000 == 0) {
        Serial.printf("\n[LOOP] Iteration: %lu | Free heap: %u bytes | Uptime: %lu s\n",
                     loopCounter, ESP.getFreeHeap(), millis() / 1000);
    }
    
    mqttServiceLoop();
    eventManagerLoop();
    audioServiceLoop();
    cameraServiceLoop();
    
    unsigned long now = millis();
    
    // === Handle Events from Event Manager ===
    while (eventAvailable()) {
        Event evt = eventPop();
        
        Serial.println("\n┌──────────────────────────────────────┐");
        Serial.println("│   EVENT HANDLER                      │");
        Serial.println("└──────────────────────────────────────┘");
        
        switch (evt.type) {
            case EVENT_BUTTON_SHORT_PRESS:
                Serial.println("[HANDLER] 🔔 Processing SHORT PRESS");
                Serial.println("[HANDLER] → Playing ding-dong sound");
                playDingDong();
                Serial.println("[HANDLER] → Capturing guest photo");
                captureGuestPhoto();
                Serial.println("[HANDLER] ✓ Short press handled\n");
                break;
                
            case EVENT_BUTTON_LONG_PRESS:
                Serial.println("[HANDLER] 🎤 Processing LONG PRESS");
                Serial.println("[HANDLER] → Starting voice recording");
                startVoiceNoteRecording();
                Serial.println("[HANDLER] ✓ Recording started\n");
                break;
                
            case EVENT_BUTTON_LONG_RELEASE:
                Serial.println("[HANDLER] 🛑 Processing LONG RELEASE");
                Serial.println("[HANDLER] → Stopping voice recording");
                stopVoiceNoteRecording();
                Serial.println("[HANDLER] ✓ Recording stopped\n");
                break;
                
            default:
                Serial.printf("[HANDLER] ⚠ Unknown event type: %d\n\n", evt.type);
                break;
        }
    }
    
    // === PIR Alert System ===
    PIRAlertLevel alertLevel = checkPIRAlertLevel();
    handlePIRAlert(alertLevel);
    
    // === Temperature Reading (every 5 minutes) ===
    if (now - lastTempRead >= TEMP_READ_INTERVAL_MS) {
        lastTempRead = now;
        Serial.println("\n[TEMP] Reading environment temperature...");
        readEnvironmentTemperature();
    }
    
    delay(5);
}
