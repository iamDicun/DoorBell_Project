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

// Sensor reading intervals
static unsigned long lastMotionCheck = 0;
static unsigned long lastTelemetryPublish = 0;
static bool lastMotionState = false;

#define MOTION_CHECK_INTERVAL    500    // Check PIR every 500ms
#define TELEMETRY_PUBLISH_INTERVAL 10000 // Publish full telemetry every 10s

static void printBanner() {
    Serial.println();
    Serial.println("=== ESP32-S3 Headless Doorbell ===");
}

void doorbellSetup() {
    printBanner();

    // Initialize sensor pins
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

    Serial.println("Doorbell services initialised");
}

void doorbellLoop() {
    mqttServiceLoop();
    eventManagerLoop();
    audioServiceLoop();
    cameraServiceLoop();
    
    unsigned long now = millis();
    
    // Check for motion and publish immediately if state changes
    if (now - lastMotionCheck >= MOTION_CHECK_INTERVAL) {
        lastMotionCheck = now;
        
        bool motionDetected = readPIRSensor();
        
        // Publish when motion state changes
        if (motionDetected != lastMotionState) {
            lastMotionState = motionDetected;
            mqttPublishMotion(motionDetected);
            Serial.printf("[PIR] Motion %s\n", motionDetected ? "DETECTED" : "cleared");
        }
    }
    
    // Publish comprehensive telemetry periodically
    if (now - lastTelemetryPublish >= TELEMETRY_PUBLISH_INTERVAL) {
        lastTelemetryPublish = now;
        
        // Read all sensors
        bool motion = readPIRSensor();
        float temperature = readTemperatureCelsius();
        float distance = readDistanceCm();
        
        // Publish individual sensor data
        mqttPublishTemperature(temperature);
        mqttPublishDistance(distance);
        
        // Publish comprehensive telemetry
        mqttPublishTelemetry(motion, temperature, distance);
        
        Serial.printf("[Telemetry] Motion=%d, Temp=%.1f°C, Distance=%.1fcm\n", 
                     motion, temperature, distance);
    }
}
