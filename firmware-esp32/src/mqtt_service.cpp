#include "mqtt_service.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "audio_service.h"
#include "audio.h"
#include "doorbell_features.h"
#include <ArduinoJson.h>
#include <time.h>

static WiFiClientSecure secureClient;
static PubSubClient mqttClient(secureClient);
static unsigned long lastHeartbeat = 0;
static unsigned long lastReconnectAttempt = 0;
static char deviceId[32];
static int reconnectCount = 0;

// Device settings (synced from database via MQTT)
static DeviceSettings deviceSettings = {
    .alarm_enabled = false,
    .do_not_disturb = false,
    .speaker_volume = 75,
    .pir_enabled = true,
    .notifications_enabled = true,
    .alarm_auto_play = true,
    .temp_enabled = true
};

// Helper functions
unsigned long getTimestamp() {
    time_t now = time(nullptr);
    if (now < 1000000000) {
        // Time not synced yet, return 0 as fallback
        return 0;
    }
    return (unsigned long)now;
}

const char* getDeviceId() {
    if (deviceId[0] == '\0') {
        snprintf(deviceId, sizeof(deviceId), "ESP32_%08X", (uint32_t)ESP.getEfuseMac());
    }
    return deviceId;
}

const DeviceSettings& getDeviceSettings() {
    return deviceSettings;
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
    IPAddress primaryDNS(8, 8, 8, 8);       // Google DNS
    IPAddress secondaryDNS(8, 8, 4, 4);     // Google DNS backup
    
    Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
    Serial.println("[WiFi] Setting custom DNS: 8.8.8.8, 8.8.4.4");
    
    // Configure DNS before connecting
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, primaryDNS, secondaryDNS);
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
    
    // Initialize NTP time sync
    Serial.println("\n[TIME] Syncing time with NTP server...");
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // GMT+7 for Vietnam
    
    int ntpRetries = 0;
    while (time(nullptr) < 1000000000 && ntpRetries < 20) {
        delay(500);
        Serial.print(".");
        ntpRetries++;
    }
    Serial.println();
    
    time_t now = time(nullptr);
    if (now > 1000000000) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        Serial.println("✓ [TIME] NTP sync successful!");
        Serial.printf("  Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        Serial.printf("  Unix timestamp: %lu\n", now);
    } else {
        Serial.println("✗ [TIME] NTP sync failed (will use uptime as fallback)");
    }
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
        
        // Request settings sync from Node-RED after successful connection
        Serial.println("[MQTT] Requesting initial settings from server...");
        delay(1000); // Wait 1 second for MQTT to stabilize
        
        // Publish request to fetch settings
        const char* requestPayload = "{\"device_id\":\"ESP32_DOORBELL\",\"action\":\"request_settings\"}";
        if (mqttClient.publish("doorbell/cmd/request_settings", requestPayload, false)) {
            Serial.println("✓ [MQTT] Settings request sent");
        } else {
            Serial.println("✗ [MQTT] Failed to send settings request");
        }
        
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
    
    // Heartbeat removed - use TOPIC_SENSOR_TEMP for device health monitoring
    // TODO: Implement telemetry if needed in future
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

void mqttHandleSpeakCommand(const char* jsonPayload) {
    Serial.println("\n========================================");
    Serial.println("[MQTT] 🔊 Processing SPEAK command");
    Serial.println("========================================");
    Serial.printf("[MQTT] Payload: %s\n", jsonPayload);
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    
    if (error) {
        Serial.printf("[MQTT] ✗ JSON parse error: %s\n", error.c_str());
        return;
    }
    
    const char* audioUrl = doc["audio_url"];
    int volume = doc["volume"] | 80;
    
    if (!audioUrl) {
        Serial.println("[MQTT] ✗ No audio_url in payload");
        return;
    }
    
    Serial.printf("[MQTT] ✓ Audio URL: %s\n", audioUrl);
    Serial.printf("[MQTT] ✓ Volume: %d%%\n", volume);
    
    // Set volume (0-100 to 0.0-1.0)
    float volumeFloat = volume / 100.0f;
    setVolumeLevel(volumeFloat);
    
    // Play audio from URL
    Serial.println("[MQTT] ▶️ Starting audio playback...");
    playUrl(audioUrl);
    
    Serial.println("========================================\n");
}

void mqttHandleCommandPayload(const char* jsonPayload) {
    Serial.printf("[MQTT] command received: %s\n", jsonPayload);
    
    StaticJsonDocument<512> doc;
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
        int duration = doc["duration"] | 0;
        activateAlarm(duration);
    }
    else if (strcmp(action, "OFF") == 0) {
        Serial.println("[MQTT] Command: Turn alarm OFF");
        deactivateAlarm();
    }
    // Capture photo - Luong 5: Remote snapshot
    else if (strcmp(action, "capture") == 0) {
        Serial.println("[MQTT] Command: Capture snapshot");
        captureSnapshotPhoto();
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
    // Settings updated notification - Trigger fetch
    else if (strcmp(action, "settings_updated") == 0) {
        Serial.println("[MQTT] Settings updated on server. Requesting new settings...");
        mqttPublishJson("doorbell/cmd/request_settings", "{\"device_id\":\"ESP32\",\"action\":\"get_settings\"}");
    }
    else if (strcmp(action, "sync_settings") == 0) {
        // Settings sync from Node-RED
        Serial.println("\n========================================");
        Serial.println("[SETTINGS] 🔄 Syncing settings from server...");
        Serial.println("========================================");
        
        bool settingsChanged = false;
        
        // Update settings from payload
        if (doc.containsKey("speaker_volume")) {
            int newVolume = doc["speaker_volume"];
            if (deviceSettings.speaker_volume != newVolume) {
                deviceSettings.speaker_volume = newVolume;
                // Apply volume immediately
                float volumeNormalized = deviceSettings.speaker_volume / 100.0f;
                setVolumeLevel(volumeNormalized);
                Serial.printf("[Settings] ✓ Speaker volume: %d%% (applied)\n", deviceSettings.speaker_volume);
                settingsChanged = true;
            }
        }
        
        if (doc.containsKey("pir_enabled")) {
            bool newPir = doc["pir_enabled"];
            if (deviceSettings.pir_enabled != newPir) {
                deviceSettings.pir_enabled = newPir;
                Serial.printf("[Settings] ✓ PIR enabled: %s\n", deviceSettings.pir_enabled ? "YES" : "NO");
                settingsChanged = true;
            }
        }
        
        if (doc.containsKey("alarm_enabled")) {
            bool newAlarm = doc["alarm_enabled"];
            if (deviceSettings.alarm_enabled != newAlarm) {
                deviceSettings.alarm_enabled = newAlarm;
                Serial.printf("[Settings] ✓ Alarm enabled: %s\n", deviceSettings.alarm_enabled ? "YES" : "NO");
                settingsChanged = true;
            }
        }
        
        if (doc.containsKey("notifications_enabled")) {
            bool newNotif = doc["notifications_enabled"];
            if (deviceSettings.notifications_enabled != newNotif) {
                deviceSettings.notifications_enabled = newNotif;
                Serial.printf("[Settings] ✓ Notifications enabled: %s\n", deviceSettings.notifications_enabled ? "YES" : "NO");
                settingsChanged = true;
            }
        }
        
        if (doc.containsKey("do_not_disturb")) {
            bool newDnd = doc["do_not_disturb"];
            if (deviceSettings.do_not_disturb != newDnd) {
                deviceSettings.do_not_disturb = newDnd;
                Serial.printf("[Settings] ✓ Do Not Disturb: %s\n", deviceSettings.do_not_disturb ? "YES" : "NO");
                settingsChanged = true;
            }
        }
        
        if (doc.containsKey("alarm_auto_play")) {
            bool newAutoPlay = doc["alarm_auto_play"];
            if (deviceSettings.alarm_auto_play != newAutoPlay) {
                deviceSettings.alarm_auto_play = newAutoPlay;
                Serial.printf("[Settings] ✓ Alarm auto-play: %s\n", deviceSettings.alarm_auto_play ? "YES" : "NO");
                settingsChanged = true;
            }
        }

        if (doc.containsKey("temp_enabled")) {
            bool newTemp = doc["temp_enabled"];
            if (deviceSettings.temp_enabled != newTemp) {
                deviceSettings.temp_enabled = newTemp;
                Serial.printf("[Settings] ✓ Temp sensor enabled: %s\n", deviceSettings.temp_enabled ? "YES" : "NO");
                settingsChanged = true;
            }
        }
        
        if (settingsChanged) {
            Serial.println("[Settings] 🎯 Settings applied successfully!");
        } else {
            Serial.println("[Settings] ℹ️  No changes - settings already up to date");
        }
        Serial.println("========================================\n");
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
    
    mqttPublishJson(TOPIC_SENSOR_TEMP, jsonBuffer);
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
    
    Serial.println("\n========================================");
    Serial.println("[MQTT] 📨 CALLBACK TRIGGERED!");
    Serial.printf("[MQTT] Topic: %s\n", topic);
    Serial.printf("[MQTT] Payload length: %d\n", length);
    Serial.printf("[MQTT] Message: %s\n", message);
    Serial.println("========================================");
    
    // Parse command topic and handle accordingly
    // Check for explicit topics first
    if (strcmp(topic, "doorbell/cmd/settings") == 0) {
        Serial.println("[MQTT] ⚙️ Settings sync command detected (Explicit Topic)");
        mqttHandleCommandPayload(message);
    }
    else if (strcmp(topic, "doorbell/cmd/siren") == 0) {
        Serial.println("[MQTT] 🚨 Siren command detected (Explicit Topic)");
        mqttHandleCommandPayload(message);
    }
    else if (strncmp(topic, "doorbell/cmd/", 13) == 0) {
        const char* cmdType = topic + 13; // Get command type after "doorbell/cmd/"
        
        Serial.printf("[MQTT] Command type: %s\n", cmdType);
        
        // Handle speak command separately
        if (strcmp(cmdType, "speak") == 0) {
            Serial.println("[MQTT] 🔊 Speak command detected");
            mqttHandleSpeakCommand(message);
        }
        // Handle settings sync separately
        else if (strcmp(cmdType, "settings") == 0) {
            Serial.println("[MQTT] ⚙️ Settings sync command detected (Wildcard)");
            mqttHandleCommandPayload(message);
        }
        // Handle other commands
        else {
            mqttHandleCommandPayload(message);
        }
    } else {
        Serial.printf("[MQTT] ⚠️ Unknown topic pattern: %s\n", topic);
    }
}

bool mqttReconnect() {
    static int reconnectAttempts = 0;
    
    // Check WiFi first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[MQTT] ✗ WiFi disconnected! Cannot connect to MQTT.");
        Serial.printf("[WiFi] Status: %d\n", WiFi.status());
        Serial.println("[WiFi] Attempting to reconnect WiFi...");
        
        // Attempt to reconnect WiFi and wait
        WiFi.reconnect();
        
        int wifiAttempts = 0;
        while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
            delay(500);
            Serial.print(".");
            wifiAttempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("✓ [WiFi] Reconnected successfully!");
            Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
            // Continue to MQTT connection below
        } else {
            Serial.println("✗ [WiFi] Failed to reconnect");
            Serial.printf("[WiFi] Status: %d\n", WiFi.status());
            return false;
        }
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
        
        // Subscribe to command topics explicitly
        // Wildcard subscription can sometimes be unreliable or filtered by broker policies
        if (mqttClient.subscribe("doorbell/cmd/settings")) {
            Serial.println("✓ [MQTT] Subscribed to doorbell/cmd/settings");
        } else {
            Serial.println("✗ [MQTT] Failed to subscribe to doorbell/cmd/settings");
        }

        if (mqttClient.subscribe("doorbell/cmd/siren")) {
            Serial.println("✓ [MQTT] Subscribed to doorbell/cmd/siren");
        }
        
        mqttClient.subscribe("doorbell/cmd/speak");
        mqttClient.subscribe("doorbell/cmd/snapshot");
        
        // Removed wildcard subscription to avoid conflicts
        // if (mqttClient.subscribe(MQTT_TOPIC_COMMAND)) { ... }
        
        Serial.println("✓ [MQTT] Subscribed to explicit topics");
        
        Serial.println("✓ [MQTT] Connection established");
        
        // Request current settings from backend after reconnection
        Serial.println("[MQTT] Requesting current settings from backend...");
        delay(1000); // Wait for MQTT connection to stabilize
        
        if (mqttClient.publish("doorbell/cmd/request_settings", "{\"device_id\":\"ESP32_DOORBELL\",\"action\":\"request_settings\"}", false)) {
            Serial.println("✓ [MQTT] Settings request sent on reconnection");
        } else {
            Serial.println("✗ [MQTT] Failed to send settings request on reconnection");
        }
        
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
