#pragma once

#include <Arduino.h>

// Device settings structure (synced from database)
struct DeviceSettings {
    bool alarm_enabled;
    bool do_not_disturb;
    int speaker_volume;        // 0-100
    bool pir_enabled;
    bool notifications_enabled;
    bool alarm_auto_play;
};

// Get current device settings
const DeviceSettings& getDeviceSettings();

void mqttServiceInit();
void mqttServiceLoop();
bool mqttReconnect();
void mqttPublishJson(const char* topic, const char* payload);
void mqttHandleCommandPayload(const char* jsonPayload);

// Helper functions for metadata
unsigned long getTimestamp();
const char* getDeviceId();

// Sensor publishing functions
void mqttPublishMotion(bool motionDetected);
void mqttPublishTemperature(float temperatureC);
void mqttPublishDistance(float distanceCm);
void mqttPublishTelemetry(bool motion, float temperature, float distance);
