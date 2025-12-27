#pragma once

#include <Arduino.h>

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
