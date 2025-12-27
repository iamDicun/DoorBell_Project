#include "mqtt_service.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "audio_service.h"
#include "doorbell_features.h"
#include <ArduinoJson.h>

static WiFiClientSecure secureClient;
static PubSubClient mqttClient(secureClient);
static unsigned long lastHeartbeat = 0;
static unsigned long lastReconnectAttempt = 0;
static char deviceId[32];
static int reconnectCount = 0;

// Helper functions
unsigned long getTimestamp() {
    return millis() / 1000; // Unix-like timestamp in seconds (uptime-based)
}

const char* getDeviceId() {
    if (deviceId[0] == '\0') {
        snprintf(deviceId, sizeof(deviceId), "ESP32_%08X", (uint32_t)ESP.getEfuseMac());
    }
    return deviceId;
}

// Forward declaration
static void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttReconnect();

void mqttServiceInit() {
    Serial.println("\n========================================");
    Serial.println("[MQTT] Initializing MQTT Service");
    Serial.println("========================================");
    
    // Configure WiFi
    secureClient.setInsecure();
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("ESP32-Doorbell");
    
    // Set Google DNS servers (fixes DNS resolution issues)
    // IPAddress primaryDNS(8, 8, 8, 8);       // Google DNS
    // IPAddress secondaryDNS(8, 8, 4, 4);     // Google DNS backup
    // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, primaryDNS, secondaryDNS);
    
    Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
    Serial.println("[WiFi] Using custom DNS: 8.8.8.8, 8.8.4.4");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] Connecting");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print('.');
        attempts++;
        
        // Print status every 5 attempts
        if (attempts % 5 == 0) {
            Serial.printf(" [%d/40]", attempts);
        }
    }
    
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n✗ [WiFi] FAILED TO CONNECT!");
        Serial.printf("[WiFi] Status code: %d\n", WiFi.status());
        Serial.println("[WiFi] Possible reasons:");
        Serial.println("  1. Wrong SSID or password");
        Serial.println("  2. WiFi router too far");
        Serial.println("  3. 5GHz network (ESP32 only supports 2.4GHz)");
        Serial.println("========================================\n");
        return;
    }
    
    Serial.println("\n✓ [WiFi] CONNECTED!");
    Serial.println("----------------------------------------");
    Serial.printf("  IP Address  : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  Subnet Mask : %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("  Gateway     : %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("  DNS Server  : %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("  MAC Address : %s\n", WiFi.macAddress().c_str());
    Serial.printf("  RSSI        : %d dBm\n", WiFi.RSSI());
    Serial.println("----------------------------------------");
    
    // Test DNS resolution
    Serial.println("\n[MQTT] Testing DNS resolution...");
    Serial.printf("[MQTT] Broker hostname: %s\n", MQTT_BROKER);
    
    IPAddress mqttIP;
    if (WiFi.hostByName(MQTT_BROKER, mqttIP)) {
        Serial.printf("✓ [MQTT] DNS resolved to: %s\n", mqttIP.toString().c_str());
    } else {
        Serial.println("✗ [MQTT] DNS RESOLUTION FAILED!");
        Serial.println("[MQTT] Possible reasons:");
        Serial.println("  1. No internet connection");
        Serial.println("  2. DNS server not responding");
        Serial.println("  3. Invalid broker hostname");
        Serial.println("========================================\n");
        return;
    }
    
    // Configure MQTT
    Serial.println("\n[MQTT] Configuring MQTT client...");
    Serial.printf("[MQTT] Broker: %s:%d\n", MQTT_BROKER, MQTT_PORT_TLS);
    Serial.printf("[MQTT] Username: %s\n", MQTT_USERNAME);
    Serial.printf("[MQTT] Using TLS: YES (insecure mode)\n");
    
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT_TLS);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    mqttClient.setKeepAlive(60);
    
    // Initial connection
    Serial.println("\n[MQTT] Attempting initial connection...");
    if (mqttReconnect()) {
        Serial.println("✓ [MQTT] CONNECTED TO HIVEMQ!");
        Serial.println("========================================\n");
    } else {
        Serial.println("✗ [MQTT] INITIAL CONNECTION FAILED");
        Serial.println("[MQTT] Will retry in loop...");
        Serial.println("========================================\n");
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
        
        // Build telemetry JSON with enhanced metadata
        char telemetry[384];
        snprintf(telemetry, sizeof(telemetry),
                 "{\"status\":\"online\",\"heap\":%u,\"rssi\":%d,\"uptime\":%lu,"
                 "\"firmware_version\":\"1.0.0\",\"ip\":\"%s\",\"reconnect_count\":%d,"
                 "\"device_id\":\"%s\",\"timestamp\":%lu}",
                 ESP.getFreeHeap(),
                 WiFi.RSSI(),
                 millis() / 1000,
                 WiFi.localIP().toString().c_str(),
                 reconnectCount,
                 getDeviceId(),
                 getTimestamp());
        
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
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    
    if (error) {
        Serial.printf("[MQTT] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    const char* action = doc["action"];
    if (!action) {
        Serial.println("[MQTT] No action field");
        return;
    }
    
    Serial.printf("[MQTT] Command received: %s\n", action);
    
    // Alarm ON/OFF - Luong 2: Backend control alarm
    if (strcmp(action, "ON") == 0) {
        Serial.println("[MQTT] Command: Turn alarm ON");
        activateAlarm();
    }
    else if (strcmp(action, "OFF") == 0) {
        Serial.println("[MQTT] Command: Turn alarm OFF");
        deactivateAlarm();
    }
    // Capture photo - Luong 5: Remote snapshot
    else if (strcmp(action, "capture") == 0) {
        Serial.println("[MQTT] Command: Capture photo");
        captureGuestPhoto();
    }
    // Legacy command handlers (kept for compatibility)
    else if (strcmp(action, "play_chime") == 0) {
        playDingDong();
    }
    else if (strcmp(action, "play_chime_1") == 0) {
        playDingDong();
    }
    else if (strcmp(action, "play_chime_2") == 0) {
        playDingDong2();
    }
    else if (strcmp(action, "play_chime_3") == 0) {
        playDingDong3();
    }
    else if (strcmp(action, "play_chime_4") == 0) {
        playDingDong4();
    }
    else if (strcmp(action, "set_volume") == 0) {
        float volume = doc["value"] | 0.5f;
        setVolumeLevel(volume);
    }
    else if (strcmp(action, "two_way_audio") == 0) {
        bool state = doc["state"] | false;
        if (state) {
            startTwoWayAudio();
        } else {
            stopTwoWayAudio();
        }
    }
    else if (strcmp(action, "capture_burst") == 0) {
        captureSecurityBurst();
    }
    else {
        Serial.printf("[MQTT] Unknown action: %s\n", action);
    }
    
    // Handle file playback - Luong 5: Play audio file
    const char* file = doc["file"];
    if (file) {
        Serial.printf("[MQTT] Command: Play file %s\n", file);
        playSampleMessage(file);
    }
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
    char jsonBuffer[192];
    float temp = round(temperatureC * 10) / 10.0; // Round to 1 decimal
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"value\":%.1f,\"unit\":\"C\",\"device_id\":\"%s\",\"timestamp\":%lu}",
             temp,
             getDeviceId(),
             getTimestamp());
    
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

bool mqttReconnect() {
    static int reconnectAttempts = 0;
    
    // Check WiFi first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[MQTT] ✗ WiFi disconnected! Cannot connect to MQTT.");
        Serial.printf("[WiFi] Status: %d\n", WiFi.status());
        Serial.println("[WiFi] Attempting to reconnect WiFi...");
        WiFi.reconnect();
        return false;
    }
    
    Serial.println("\n┌──────────────────────────────────────┐");
    Serial.printf("│ MQTT Connection Attempt #%-3d         │\n", ++reconnectAttempts);
    Serial.println("└──────────────────────────────────────┘");
    
    Serial.printf("[MQTT] WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[MQTT] WiFi RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("[MQTT] Broker: %s:%d\n", MQTT_BROKER, MQTT_PORT_TLS);
    
    // Create unique client ID
    String clientId = "ESP32-Doorbell-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);
    
    Serial.printf("[MQTT] Client ID: %s\n", clientId.c_str());
    Serial.println("[MQTT] Connecting...");
    
    unsigned long connectStart = millis();
    
    // Attempt connection with credentials
    bool connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
    
    unsigned long connectDuration = millis() - connectStart;
    
    if (connected) {
        reconnectCount++;
        reconnectAttempts = 0;
        Serial.println("\n✓✓✓ [MQTT] CONNECTION SUCCESSFUL! ✓✓✓");
        Serial.printf("[MQTT] Connection time: %lu ms\n", connectDuration);
        
        // Subscribe to command topic
        if (mqttClient.subscribe(MQTT_TOPIC_COMMAND)) {
            Serial.printf("✓ [MQTT] Subscribed to %s\n", MQTT_TOPIC_COMMAND);
        }
        
        // Publish online status with details
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "{\"status\":\"online\",\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu}",
                 WiFi.localIP().toString().c_str(),
                 WiFi.RSSI(),
                 millis() / 1000);
        mqttPublishJson(MQTT_TOPIC_STATUS, msg);
        
        Serial.println("✓ [MQTT] Published online status");
        Serial.println("========================================\n");
        
        return true;
    } else {
        int state = mqttClient.state();
        Serial.println("\n✗✗✗ [MQTT] CONNECTION FAILED! ✗✗✗");
        Serial.printf("[MQTT] Connection time: %lu ms\n", connectDuration);
        Serial.printf("[MQTT] Error code: %d\n", state);
        Serial.println("[MQTT] Error meanings:");
        Serial.println("  -4 : Connection timeout");
        Serial.println("  -3 : Connection lost");
        Serial.println("  -2 : Connect failed (network unreachable)");
        Serial.println("  -1 : Disconnected");
        Serial.println("   1 : Bad protocol");
        Serial.println("   2 : Bad client ID");
        Serial.println("   3 : Unavailable");
        Serial.println("   4 : Bad credentials");
        Serial.println("   5 : Unauthorized");
        
        if (state == -2) {
            Serial.println("\n[MQTT] ⚠ Network unreachable - Possible causes:");
            Serial.println("  1. Firewall blocking port 8883");
            Serial.println("  2. No internet connection (check router)");
            Serial.println("  3. HiveMQ server down");
            Serial.println("  4. DNS issue");
            
            // Test DNS resolution
            Serial.println("\n[MQTT] Testing DNS resolution...");
            IPAddress mqttIP;
            if (WiFi.hostByName(MQTT_BROKER, mqttIP)) {
                Serial.printf("✓ [MQTT] DNS resolved to: %s\n", mqttIP.toString().c_str());
            } else {
                Serial.println("✗ [MQTT] DNS RESOLUTION FAILED!");
            }
        }
        
        Serial.println("========================================\n");
        return false;
    }
}
