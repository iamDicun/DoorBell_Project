#include "doorbell_features.h"
#include "config.h"
#include "audio.h"
#include "camera_module.h"
#include "mqtt_service.h"
#include "upload_client.h"
#include "sensor_utils.h"
#include <SPIFFS.h>

// --- Global State Variables ---
static bool isAlarmActive = false;
static bool isTwoWayAudioActive = false;
static bool isRecordingVoiceNote = false;

// PIR detection history
static unsigned long pirDetectionTimes[10];
static int pirDetectionCount = 0;
static unsigned long lastPIRCheck = 0;
static unsigned long lastPIRDetection = 0;  // For debouncing

#define PIR_DETECTION_DEBOUNCE_MS 3000  // Minimum 3s between detections

// Helper: normalize path for SPIFFS (ensure starts with '/' and strip data/ prefix)
static String normalizeSPIFFSPath(const char* path) {
    if (!path || strlen(path) == 0) {
        Serial.println("[AUDIO] ERROR: Null or empty path");
        return String("/");
    }
    
    String p = String(path);
    
    // Remove leading ./data/ or data/
    if (p.startsWith("./data/")) {
        p = p.substring(7); // Remove "./data/"
    } else if (p.startsWith("data/")) {
        p = p.substring(5); // Remove "data/"
    } else if (p.startsWith("./")) {
        p = p.substring(2); // Remove "./"
    }
    
    // Ensure starts with /
    if (!p.startsWith("/")) {
        p = String("/") + p;
    }
    
    return p;
}

// Helper function to play audio from SPIFFS
static void playAudioFile(const char* filepath) {
    Serial.println("\n========== AUDIO PLAYBACK ==========");
    Serial.printf("[AUDIO] Original path: '%s'\n", filepath ? filepath : "(null)");
    
    String normalizedPath = normalizeSPIFFSPath(filepath);
    Serial.printf("[AUDIO] Normalized path: '%s'\n", normalizedPath.c_str());
    
    // Check if SPIFFS is mounted
    if (!SPIFFS.begin(false)) {
        Serial.println("[AUDIO] ERROR: SPIFFS not mounted!");
        playTestTone();
        return;
    }
    
    // Check if file exists
    if (!SPIFFS.exists(normalizedPath.c_str())) {
        Serial.printf("[AUDIO] ❌ File not found: %s\n", normalizedPath.c_str());
        Serial.println("[AUDIO] Using fallback tone");
        Serial.println("====================================\n");
        playTestTone();
        return;
    }
    
    // Try to open file
    File audioFile = SPIFFS.open(normalizedPath.c_str(), "r");
    if (!audioFile) {
        Serial.printf("[AUDIO] ❌ Failed to open: %s\n", normalizedPath.c_str());
        Serial.println("[AUDIO] Using fallback tone");
        Serial.println("====================================\n");
        playTestTone();
        return;
    }
    
    size_t fileSize = audioFile.size();
    Serial.printf("[AUDIO] ✓ File opened: %s (%u bytes)\n", normalizedPath.c_str(), fileSize);
    
    // Get upload buffer
    uint8_t* buffer = getUploadBuffer();
    if (!buffer) {
        Serial.println("[AUDIO] ❌ Failed to allocate buffer");
        audioFile.close();
        playTestTone();
        return;
    }
    
    // Read file content
    size_t bytesRead = audioFile.read(buffer, fileSize);
    audioFile.close();
    
    if (bytesRead != fileSize) {
        Serial.printf("[AUDIO] ❌ Read error: %u/%u bytes\n", bytesRead, fileSize);
        playTestTone();
        return;
    }
    
    Serial.printf("[AUDIO] ✓ Read %u bytes successfully\n", bytesRead);
    
    // Set buffer and play
    setUploadedBytes(bytesRead);
    setUploadReady(true);
    
    Serial.println("[AUDIO] ✓ Starting playback...");
    Serial.println("====================================\n");
    
    playUploadedAudio();
}

// --- Button & Door Features ---

void playDingDong() {
    Serial.println("[FEATURE] Playing Ding-Dong (Style 1)");
    playAudioFile(AUDIO_DING_DONG);
    
    char msg[192];
    snprintf(msg, sizeof(msg), 
             "{\"event\":\"press\",\"chime\":\"style1\",\"device_id\":\"%s\",\"timestamp\":%lu}",
             getDeviceId(), getTimestamp());
    mqttPublishJson(MQTT_TOPIC_STATUS, msg);
}

void playDingDong2() {
    Serial.println("[FEATURE] Playing Ding-Dong (Style 2)");
    playAudioFile(AUDIO_DING_DONG_2);
    
    char msg[192];
    snprintf(msg, sizeof(msg), 
             "{\"event\":\"press\",\"chime\":\"style2\",\"device_id\":\"%s\",\"timestamp\":%lu}",
             getDeviceId(), getTimestamp());
    mqttPublishJson(MQTT_TOPIC_STATUS, msg);
}

void playDingDong3() {
    Serial.println("[FEATURE] Playing Ding-Dong (Style 3)");
    
    if (SPIFFS.exists(AUDIO_DING_DONG_3)) {
        File audioFile = SPIFFS.open(AUDIO_DING_DONG_3, "r");
        if (audioFile) {
            size_t fileSize = audioFile.size();
            Serial.printf("[FEATURE] Loading %s (%u bytes)\n", AUDIO_DING_DONG_3, fileSize);
            
            uint8_t* buffer = getUploadBuffer();
            if (buffer && fileSize <= 2 * 1024 * 1024) {
                size_t bytesRead = audioFile.read(buffer, fileSize);
                audioFile.close();
                
                if (bytesRead == fileSize) {
                    setUploadedBytes(bytesRead);
                    setUploadReady(true);
                    playUploadedAudio();
                } else {
                    Serial.printf("[FEATURE] Read error: %u/%u bytes\n", bytesRead, fileSize);
                    playTestTone();
                }
            } else {
                Serial.println("[FEATURE] Buffer allocation failed or file too large");
                audioFile.close();
                playTestTone();
            }
        } else {
            Serial.println("[FEATURE] Failed to open file");
            playTestTone();
        }
    } else {
        Serial.println("[FEATURE] File not found, using fallback tone");
        playTestTone();
    }
    
    char msg[192];
    snprintf(msg, sizeof(msg), 
             "{\"event\":\"press\",\"chime\":\"style3\",\"device_id\":\"%s\",\"timestamp\":%lu}",
             getDeviceId(), getTimestamp());
    mqttPublishJson(MQTT_TOPIC_STATUS, msg);
}

void startVoiceNoteRecording() {
    if (isRecordingVoiceNote) return;
    
    Serial.println("[FEATURE] Starting voice note recording");
    isRecordingVoiceNote = true;
    
    startRecording();
}

void stopVoiceNoteRecording() {
    if (!isRecordingVoiceNote) return;
    
    Serial.println("[FEATURE] Stopping voice note recording");
    stopRecording();
    isRecordingVoiceNote = false;
    
    // Wait for recording to complete
    delay(100);
    
    if (isRecordingReady() && getRecordedBytes() > 0) {
        // Upload to server
        Serial.println("[FEATURE] Uploading voice note...");
        
        // Create WAV header
        uint8_t wavHeader[44];
        uint32_t dataSize = getRecordedBytes();
        uint32_t fileSize = dataSize + 36;
        
        memcpy(wavHeader, "RIFF", 4);
        memcpy(wavHeader + 4, &fileSize, 4);
        memcpy(wavHeader + 8, "WAVEfmt ", 8);
        uint32_t subchunk1Size = 16;
        memcpy(wavHeader + 16, &subchunk1Size, 4);
        uint16_t audioFormat = 1;
        memcpy(wavHeader + 20, &audioFormat, 2);
        uint16_t numChannels = 1;
        memcpy(wavHeader + 22, &numChannels, 2);
        uint32_t sampleRate = MIC_SAMPLE_RATE;
        memcpy(wavHeader + 24, &sampleRate, 4);
        uint32_t byteRate = MIC_SAMPLE_RATE * 2;
        memcpy(wavHeader + 28, &byteRate, 4);
        uint16_t blockAlign = 2;
        memcpy(wavHeader + 32, &blockAlign, 2);
        uint16_t bitsPerSample = 16;
        memcpy(wavHeader + 34, &bitsPerSample, 2);
        memcpy(wavHeader + 36, "data", 4);
        memcpy(wavHeader + 40, &dataSize, 4);
        
        // Allocate buffer for complete WAV file
        uint8_t* wavBuffer = (uint8_t*)malloc(44 + dataSize);
        if (wavBuffer) {
            memcpy(wavBuffer, wavHeader, 44);
            memcpy(wavBuffer + 44, (uint8_t*)getRecordBuffer(), dataSize);
            
            bool uploaded = uploadWithRetry(ENDPOINT_VOICE_NOTE, wavBuffer, 44 + dataSize, "audio/wav", 3,
                                          "voice_note", getTimestamp());
            free(wavBuffer);
            
            if (uploaded) {
                Serial.println("[FEATURE] Voice note uploaded successfully");
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "{\"event\":\"voice_note_uploaded\",\"size\":%u,\"duration_seconds\":%.1f,"
                         "\"device_id\":\"%s\",\"timestamp\":%lu}",
                         44 + dataSize,
                         (float)dataSize / (MIC_SAMPLE_RATE * 2),
                         getDeviceId(),
                         getTimestamp());
                mqttPublishJson(MQTT_TOPIC_STATUS, msg);
            } else {
                Serial.println("[FEATURE] Voice note upload failed");
            }
        }
    }
}

// --- Camera & Security Features ---

void captureGuestPhoto() {
    Serial.println("[FEATURE] Capturing guest photo");
    
    camera_fb_t* fb = captureFrame();
    if (fb) {
        Serial.printf("[FEATURE] Photo captured: %u bytes\n", fb->len);
        
        // Upload photo with metadata headers
        bool uploaded = uploadWithRetry(ENDPOINT_GUEST_IMG, fb->buf, fb->len, "image/jpeg", 3, 
                                        "doorbell_press", getTimestamp());
        
        if (uploaded) {
            Serial.println("[FEATURE] Guest photo uploaded");
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "{\"event\":\"guest_photo_captured\",\"size\":%u,"
                     "\"device_id\":\"%s\",\"timestamp\":%lu}",
                     fb->len,
                     getDeviceId(),
                     getTimestamp());
            mqttPublishJson(MQTT_TOPIC_STATUS, msg);
        }
        
        releaseFrame(fb);
    } else {
        Serial.println("[FEATURE] Failed to capture photo");
    }
}

void captureSecurityBurst() {
    Serial.println("[FEATURE] Capturing security burst");
    
    for (int i = 0; i < CAMERA_BURST_COUNT; i++) {
        camera_fb_t* fb = captureFrame();
        if (fb) {
            Serial.printf("[FEATURE] Burst photo %d: %u bytes\n", i + 1, fb->len);
            
            // Upload with metadata headers
            char endpoint[64];
            snprintf(endpoint, sizeof(endpoint), "%s?seq=%d", ENDPOINT_BURST_IMG, i);
            uploadWithRetry(endpoint, fb->buf, fb->len, "image/jpeg", 2, 
                          "pir_burst", getTimestamp());
            
            releaseFrame(fb);
        }
        
        if (i < CAMERA_BURST_COUNT - 1) {
            delay(CAMERA_BURST_DELAY_MS);
        }
    }
    
    // Enhanced MQTT payload with metadata
    char msg[256];
    snprintf(msg, sizeof(msg),
             "{\"status\":\"detected\",\"count\":%d,\"device_id\":\"%s\",\"timestamp\":%lu}",
             CAMERA_BURST_COUNT,
             getDeviceId(),
             getTimestamp());
    mqttPublishJson(MQTT_TOPIC_SECURITY, msg);
}

void activateAlarm() {
    if (isAlarmActive) return;
    
    Serial.println("[FEATURE] Activating alarm");
    isAlarmActive = true;
    
    // Play alarm sound from SPIFFS
    playAudioFile(AUDIO_ALARM);
    
    mqttPublishJson(MQTT_TOPIC_SECURITY, "{\"event\":\"alarm_activated\"}");
}

void deactivateAlarm() {
    if (!isAlarmActive) return;
    
    Serial.println("[FEATURE] Deactivating alarm");
    isAlarmActive = false;
    
    mqttPublishJson(MQTT_TOPIC_SECURITY, "{\"event\":\"alarm_deactivated\"}");
}

// --- Audio & Speaker Features ---

void startTwoWayAudio() {
    if (isTwoWayAudioActive) return;
    
    Serial.println("[FEATURE] Starting two-way audio");
    isTwoWayAudioActive = true;
    
    // Start microphone streaming
    setWsAudioStreaming(true);
    
    // Start speaker for incoming audio
    startWebStream();
    
    mqttPublishJson(MQTT_TOPIC_STATUS, "{\"event\":\"two_way_audio_started\"}");
}

void stopTwoWayAudio() {
    if (!isTwoWayAudioActive) return;
    
    Serial.println("[FEATURE] Stopping two-way audio");
    isTwoWayAudioActive = false;
    
    setWsAudioStreaming(false);
    stopWebStream();
    
    mqttPublishJson(MQTT_TOPIC_STATUS, "{\"event\":\"two_way_audio_stopped\"}");
}

void playSampleMessage(const char* messageType) {
    Serial.printf("[FEATURE] Playing sample message: %s\n", messageType);
    
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/%s.mp3", messageType);
    
    if (SPIFFS.exists(filepath)) {
        // Play from SPIFFS - you'll need to implement SPIFFS audio playback
        Serial.printf("[FEATURE] Playing %s from SPIFFS\n", filepath);
    } else {
        Serial.println("[FEATURE] Sample message file not found");
        playTestTone(); // Fallback
    }
}

void setVolumeLevel(float level) {
    level = constrain(level, 0.0f, 1.5f);
    setSpeakerVolume(level);
    Serial.printf("[FEATURE] Volume set to %.2f\n", level);
    
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"event\":\"volume_changed\",\"level\":%.2f}", level);
    mqttPublishJson(MQTT_TOPIC_STATUS, msg);
}

// --- Environment Features ---

void readEnvironmentTemperature() {
    float temp = readTemperatureCelsius();
    
    if (temp > -100.0f) { // Valid reading
        Serial.printf("[FEATURE] Environment temperature: %.1f°C\n", temp);
        
        // Publish to MQTT
        mqttPublishTemperature(temp);
        
        // Check for extreme temperatures
        if (temp > 45.0f || temp < -10.0f) {
            char alert[128];
            snprintf(alert, sizeof(alert), 
                     "{\"event\":\"extreme_temperature\",\"value\":%.1f,\"unit\":\"C\"}", temp);
            mqttPublishJson(MQTT_TOPIC_SECURITY, alert);
        }
    } else {
        Serial.println("[FEATURE] Failed to read temperature");
    }
}

// --- PIR Alert System ---

// Register a PIR detection event (called from event_manager on rising edge)
void registerPIRDetection(unsigned long timestamp) {
    // Debounce: Ignore detections within 3 seconds of last one
    if (timestamp - lastPIRDetection < PIR_DETECTION_DEBOUNCE_MS) {
        Serial.printf("[PIR-REG] Debounced - only %lu ms since last (need %d ms)\n",
                     timestamp - lastPIRDetection, PIR_DETECTION_DEBOUNCE_MS);
        return;
    }
    lastPIRDetection = timestamp;
    
    // Add detection to history buffer
    if (pirDetectionCount < 10) {
        pirDetectionTimes[pirDetectionCount++] = timestamp;
    } else {
        // Shift array and add new detection
        for (int i = 0; i < 9; i++) {
            pirDetectionTimes[i] = pirDetectionTimes[i + 1];
        }
        pirDetectionTimes[9] = timestamp;
    }
    Serial.printf("[PIR-REG] Detection registered at %lu ms (total count: %d)\n", 
                 timestamp, pirDetectionCount);
}

PIRAlertLevel checkPIRAlertLevel() {
    unsigned long now = millis();
    
    // Check if it's time to scan
    if (now - lastPIRCheck < PIR_SCAN_INTERVAL_MS) {
        return ALERT_NORMAL; // Not time to check yet
    }
    lastPIRCheck = now;
    
    // Count detections in the last 20 seconds (detections are registered by event_manager)
    int recentDetections = 0;
    for (int i = 0; i < pirDetectionCount; i++) {
        if (now - pirDetectionTimes[i] <= PIR_SCAN_WINDOW_MS) {
            recentDetections++;
        }
    }
    
    // Clean up old detections
    int newCount = 0;
    for (int i = 0; i < pirDetectionCount; i++) {
        if (now - pirDetectionTimes[i] <= PIR_SCAN_WINDOW_MS) {
            pirDetectionTimes[newCount++] = pirDetectionTimes[i];
        }
    }
    pirDetectionCount = newCount;
    
    // Determine alert level
    if (recentDetections >= PIR_ALERT_HIGH) {
        return ALERT_HIGH;
    } else if (recentDetections >= PIR_ALERT_MEDIUM) {
        return ALERT_MEDIUM;
    } else {
        return ALERT_NORMAL;
    }
}

void handlePIRAlert(PIRAlertLevel level) {
    static PIRAlertLevel lastLevel = ALERT_NORMAL;
    
    // Only act if level changed
    if (level == lastLevel) return;
    Serial.printf("[PIR] Current alert level: %d\n", level);
    lastLevel = level;
    
    switch (level) {
        case ALERT_HIGH:
            Serial.println("[PIR] HIGH ALERT - Suspicious activity detected!");
            captureSecurityBurst();
            activateAlarm();
            {
                char alertMsg[256];
                snprintf(alertMsg, sizeof(alertMsg),
                    "{\"event\":\"pir_alert\",\"level\":\"high\",\"message\":\"Suspicious loitering detected\",\"detections\":%d,\"device_id\":\"%s\",\"timestamp\":%lu}",
                    pirDetectionCount, getDeviceId(), getTimestamp());
                mqttPublishJson(MQTT_TOPIC_SECURITY, alertMsg);
            }
            break;
            
        case ALERT_MEDIUM:
            Serial.println("[PIR] MEDIUM ALERT - Person detected");
            captureGuestPhoto();
            {
                char alertMsg[256];
                snprintf(alertMsg, sizeof(alertMsg),
                    "{\"event\":\"pir_alert\",\"level\":\"medium\",\"message\":\"Person lingering at door\",\"detections\":%d,\"device_id\":\"%s\",\"timestamp\":%lu}",
                    pirDetectionCount, getDeviceId(), getTimestamp());
                mqttPublishJson(MQTT_TOPIC_SECURITY, alertMsg);
            }
            break;
            
        case ALERT_NORMAL:
            Serial.println("[PIR] Normal - Motion cleared");
            if (isAlarmActive) {
                deactivateAlarm();
            }
            {
                char alertMsg[256];
                snprintf(alertMsg, sizeof(alertMsg),
                    "{\"event\":\"pir_alert\",\"level\":\"normal\",\"message\":\"Motion cleared\",\"detections\":%d,\"device_id\":\"%s\",\"timestamp\":%lu}",
                    pirDetectionCount, getDeviceId(), getTimestamp());
                mqttPublishJson(MQTT_TOPIC_SECURITY, alertMsg);
            }
            break;
    }
}
