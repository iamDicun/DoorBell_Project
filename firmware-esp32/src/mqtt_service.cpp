#include "mqtt_service.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "audio_service.h"

static WiFiClientSecure secureClient;
static PubSubClient mqttClient(secureClient);
static PubSubClient mqttClient(secureClient);
static unsigned long lastHeartbeat = 0;
static unsigned long lastReconnectAttempt = 0;

// Forward declaration
static void mqttCallback(char* topic, byte* payload, unsigned int length);
static bool mqttReconnect();

void mqttServiceInit() {
    Serial.println("[MQTT] init");
    
    // Configure WiFi
    secureClient.setInsecure();
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] Connectingggggg");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print('.');
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] Failed to connect!");
        return;
    }
    
    Serial.println();
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    
    // Configure MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT_TLS);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    
    // Initial connection
    if (mqttReconnect()) {
        Serial.println("[MQTT] Connected to HiveMQ");
    } else {
        Serial.println("[MQTT] Initial connection failed");
    }
}

void mqttServiceLoop() {
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (mqttReconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        mqttClient.loop();
    }
    
    // Send heartbeat every 30 seconds
    if (millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();
        
        // Build telemetry JSON
        char telemetry[256];
        snprintf(telemetry, sizeof(telemetry),
                 "{\"status\":\"online\",\"heap\":%u,\"rssi\":%d,\"uptime\":%lu}",
                 ESP.getFreeHeap(),
                 WiFi.RSSI(),
                 millis() / 1000);
        
        mqttPublishJson(MQTT_TOPIC_HEARTBEAT, telemetry);
    }
}

void mqttPublishJson(const char* topic, const char* payload) {
    if (mqttClient.connected()) {
        if (mqttClient.publish(topic, payload)) {
            Serial.printf("[MQTT] Published to %s: %s\n", topic, payload);
        } else {
            Serial.printf("[MQTT] Failed to publish to %s\n", topic);
        }
    } else {
        Serial.printf("[MQTT] Not connected, cannot publish to %s\n", topic);
    }
}

void mqttHandleCommandPayload(const char* jsonPayload) {
    Serial.printf("[MQTT] command received: %s\n", jsonPayload);
    // TODO: Parse JSON and execute commands
    // Example: {"action":"unlock"}, {"action":"ring"}, etc.
}

// Publish motion detection status
void mqttPublishMotion(bool motionDetected) {
    char jsonBuffer[128];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"motion\":%s,\"timestamp\":%lu}",
             motionDetected ? "true" : "false",
             millis());
    
    mqttPublishJson("doorbell/sensors/motion", jsonBuffer);
}

// Publish temperature in Celsius
void mqttPublishTemperature(float temperatureC) {
    char jsonBuffer[128];
    float temp = round(temperatureC * 10) / 10.0; // Round to 1 decimal
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"temperature\":%.1f,\"unit\":\"C\",\"timestamp\":%lu}",
             temp, millis());
    
    mqttPublishJson("doorbell/sensors/temperature", jsonBuffer);
}

// Publish distance measurement
void mqttPublishDistance(float distanceCm) {
    char jsonBuffer[128];
    float dist = round(distanceCm * 10) / 10.0; // Round to 1 decimal
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"distance\":%.1f,\"unit\":\"cm\",\"timestamp\":%lu}",
             dist, millis());
    
    mqttPublishJson("doorbell/sensors/distance", jsonBuffer);
}

// Publish comprehensive telemetry data
void mqttPublishTelemetry(bool motion, float temperature, float distance) {
    char jsonBuffer[256];
    float temp = round(temperature * 10) / 10.0;
    float dist = round(distance * 10) / 10.0;
    
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"motion\":%s,\"temperature\":%.1f,\"distance\":%.1f,\"wifi_rssi\":%d,\"timestamp\":%lu}",
             motion ? "true" : "false",
             temp, dist,
             WiFi.RSSI(),
             millis());
    
    mqttPublishJson(MQTT_TOPIC_TELEMETRY, jsonBuffer);
}

// Private functions
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Create null-terminated string from payload
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.printf("[MQTT] Message on topic %s: %s\n", topic, message);
    
    // Handle command topic
    if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
        mqttHandleCommandPayload(message);
    }
}

static bool mqttReconnect() {
    Serial.println("[MQTT] Attempting connection...");
    
    // Create unique client ID
    String clientId = "ESP32-Doorbell-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);
    
    // Attempt connection with credentials
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.printf("[MQTT] Connected as %s\n", clientId.c_str());
        
        // Subscribe to command topic
        if (mqttClient.subscribe(MQTT_TOPIC_COMMAND)) {
            Serial.printf("[MQTT] Subscribed to %s\n", MQTT_TOPIC_COMMAND);
        }
        
        // Publish online status
        mqttPublishJson(MQTT_TOPIC_STATUS, "{\"status\":\"online\"}");
        
        return true;
    } else {
        Serial.printf("[MQTT] Connection failed, rc=%d\n", mqttClient.state());
        return false;
    }
}
