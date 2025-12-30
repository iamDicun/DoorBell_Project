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
#include "web_server.h"
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
    Serial.println("\n[INIT] Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("[ERR] ❌ SPIFFS init failed");
    } else {
        Serial.println("[SPIFFS] ✓ Initialized");
        
        // Get SPIFFS info
        size_t totalBytes = SPIFFS.totalBytes();
        size_t usedBytes = SPIFFS.usedBytes();
        Serial.printf("[SPIFFS] Total: %u bytes | Used: %u bytes | Free: %u bytes\n",
                     totalBytes, usedBytes, totalBytes - usedBytes);
        
        // List all files in SPIFFS
        Serial.println("\n[SPIFFS] File system contents:");
        File root = SPIFFS.open("/");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            int fileCount = 0;
            while (file) {
                Serial.printf("  %d. %-30s %8u bytes\n", 
                             ++fileCount, file.name(), file.size());
                file = root.openNextFile();
            }
            if (fileCount == 0) {
                Serial.println("  ⚠ WARNING: No files found!");
                Serial.println("  You need to upload audio files using:");
                Serial.println("  'pio run --target uploadfs' or PlatformIO menu");
            } else {
                Serial.printf("  Total: %d files\n", fileCount);
            }
        } else {
            Serial.println("  ❌ Cannot read SPIFFS root directory");
        }
        
        // Check expected audio files
        Serial.println("\n[SPIFFS] Checking expected audio files:");
        const char* expectedFiles[] = {
            AUDIO_DING_DONG,
            AUDIO_DING_DONG_2,
            AUDIO_DING_DONG_3,
            AUDIO_ALARM,
            AUDIO_PLEASE_WAIT
        };
        
        int foundCount = 0;
        for (int i = 0; i < 5; i++) {
            if (SPIFFS.exists(expectedFiles[i])) {
                File f = SPIFFS.open(expectedFiles[i], "r");
                Serial.printf("  ✓ %-30s %8u bytes\n", expectedFiles[i], f.size());
                f.close();
                foundCount++;
            } else {
                Serial.printf("  ✗ %-30s MISSING (will use fallback)\n", expectedFiles[i]);
            }
        }
        Serial.printf("  Found %d/%d expected files\n", foundCount, 5);
        Serial.println();
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

    // Initialize WiFi and Web Server
    Serial.println("\n[INIT] Starting WiFi...");
    if (!initWiFi()) {
        Serial.println("[ERR] ❌ WiFi connection failed!");
        Serial.println("[WARN] Web server and MQTT will not work without WiFi");
    } else {
        Serial.println("[WiFi] ✓ Connected successfully");
        
        // Setup Web Server
        Serial.println("[INIT] Starting Web Server...");
        setupWebServer();
        Serial.println("[WebServer] ✓ HTTP server running on port 80");
        Serial.println("[WebServer] ✓ WebSocket server running on port 81");
    }

    mqttServiceInit();
    eventManagerInit();
    audioServiceInit();
    cameraServiceInit();

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   DOORBELL READY                       ║");
    Serial.println("║   All services initialized             ║");
    Serial.println("╚════════════════════════════════════════╝\n");
}

void doorbellLoop() {
    loopCounter++;
    
    // Print loop status every 5000 iterations (approximately every 25 seconds)
    if (loopCounter % 5000 == 0) {
        Serial.printf("\n[LOOP] Iteration: %lu | Free heap: %u bytes | Uptime: %lu s\n",
                     loopCounter, ESP.getFreeHeap(), millis() / 1000);
    }
    
    // Handle Web Server & WebSocket
    webSocket.loop();
    
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
