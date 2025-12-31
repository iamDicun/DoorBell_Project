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
static unsigned long alarmStartTime = 0;
static unsigned long alarmDuration = 0;
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
    // No MQTT needed - ding-dong is just local sound
}

void playDingDong2() {
    Serial.println("[FEATURE] Playing Ding-Dong (Style 2)");
    playAudioFile(AUDIO_DING_DONG_2);
    // No MQTT needed - ding-dong is just local sound
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
    // No MQTT needed - ding-dong is just local sound
}

void startVoiceNoteRecording() {
    if (isRecordingVoiceNote) return;
    
    Serial.println("[FEATURE] Starting voice note recording");
    
    // Play start tone (middle frequency)
    playRecordingStartTone();
    delay(150); // Wait for tone to finish
    
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
            
            // Generate filename: voice_{timestamp}.wav
            unsigned long timestamp = getTimestamp();
            char filename[64];
            snprintf(filename, sizeof(filename), "voice_%lu.wav", timestamp);
            
            // Upload directly to Supabase Storage
            bool uploaded = uploadToSupabase(wavBuffer, 44 + dataSize, SUPABASE_BUCKET_AUDIO, filename);
            free(wavBuffer);
            
            if (uploaded) {
                Serial.println("[FEATURE] Voice note uploaded successfully");
                
                // Play success tone (high frequency)
                playRecordingSuccessTone();
                
                // Flow 3: Get URL from upload response and publish to TOPIC_EVT_VOICE
                const char* audioUrl = getLastUploadedUrl();
                if (audioUrl && strlen(audioUrl) > 0) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "{\"audio_url\":\"%s\",\"timestamp\":%lu,\"device_id\":\"ESP32_DOORBELL\",\"duration_ms\":%lu}", 
                             audioUrl, getTimestamp(), (unsigned long)(dataSize * 1000 / (MIC_SAMPLE_RATE * 2)));
                    mqttPublishJson(TOPIC_EVT_VOICE, msg);
                    Serial.printf("[FEATURE] Published voice note to MQTT: %s\n", audioUrl);
                } else {
                    Serial.println("[FEATURE] Warning: No audio URL returned from upload");
                }
            } else {
                Serial.println("[FEATURE] Voice note upload failed");
                
                // Play error tone (low frequency)
                playRecordingErrorTone();
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
        
        // Generate filename: timestamp_button.jpg
        unsigned long timestamp = getTimestamp();
        char filename[64];
        snprintf(filename, sizeof(filename), "%lu_button.jpg", timestamp);
        
        // Upload directly to Supabase Storage
        bool uploaded = uploadToSupabase(fb->buf, fb->len, SUPABASE_BUCKET_IMAGES, filename);
        
        if (uploaded) {
            Serial.println("[FEATURE] Guest photo uploaded to Supabase");
            
            // Get public URL from Supabase
            const char* imageUrl = getLastUploadedUrl();
            
            // Publish MQTT to doorbell/evt/button with new payload format
            char msg[512];
            if (imageUrl && strlen(imageUrl) > 0) {
                snprintf(msg, sizeof(msg),
                         "{\"image_url\":\"%s\",\"timestamp\":%lu,\"description\":\"Button press photo\"}",
                         imageUrl, timestamp);
                mqttPublishJson(TOPIC_EVT_BUTTON, msg);
                Serial.printf("[FEATURE] Published to %s: %s\n", TOPIC_EVT_BUTTON, msg);
            } else {
                Serial.println("[FEATURE] Warning: No URL returned from upload");
            }
        } else {
            Serial.println("[FEATURE] Failed to upload photo to Supabase");
        }
        
        releaseFrame(fb);
    } else {
        Serial.println("[FEATURE] Failed to capture photo");
    }
}

// Capture snapshot photo (manual via MQTT command)
void captureSnapshotPhoto() {
    Serial.println("[FEATURE] Capturing snapshot photo");
    
    camera_fb_t* fb = captureFrame();
    if (fb) {
        Serial.printf("[FEATURE] Snapshot captured: %u bytes\n", fb->len);
        
        // Generate filename: timestamp_snapshot.jpg
        unsigned long timestamp = getTimestamp();
        char filename[64];
        snprintf(filename, sizeof(filename), "%lu_snapshot.jpg", timestamp);
        
        // Upload to Supabase Storage
        bool uploaded = uploadToSupabase(fb->buf, fb->len, SUPABASE_BUCKET_IMAGES, filename);
        
        if (uploaded) {
            Serial.println("[FEATURE] Snapshot uploaded to Supabase");
            
            // Get public URL
            const char* imageUrl = getLastUploadedUrl();
            
            // Publish to doorbell/evt/snapshot
            char msg[512];
            if (imageUrl && strlen(imageUrl) > 0) {
                snprintf(msg, sizeof(msg),
                         "{\"image_url\":\"%s\",\"timestamp\":%lu,\"description\":\"Manual snapshot\"}",
                         imageUrl, timestamp);
                mqttPublishJson(TOPIC_EVT_SNAPSHOT, msg);
                Serial.printf("[FEATURE] Published to %s: %s\n", TOPIC_EVT_SNAPSHOT, msg);
            } else {
                Serial.println("[FEATURE] Warning: No URL returned from upload");
            }
        } else {
            Serial.println("[FEATURE] Failed to upload snapshot to Supabase");
        }
        
        releaseFrame(fb);
    } else {
        Serial.println("[FEATURE] Failed to capture snapshot");
    }
}

void captureSecurityBurst() {
    Serial.println("[FEATURE] Capturing security burst (PIR triggered)");
    
    unsigned long timestamp = getTimestamp();
    String imageUrls[CAMERA_BURST_COUNT];
    int successCount = 0;
    
    // Capture and upload each photo in burst
    for (int i = 0; i < CAMERA_BURST_COUNT; i++) {
        camera_fb_t* fb = captureFrame();
        if (fb) {
            Serial.printf("[FEATURE] Burst photo %d/%d: %u bytes\n", i + 1, CAMERA_BURST_COUNT, fb->len);
            
            // Generate filename: timestamp_pir_1.jpg, timestamp_pir_2.jpg, ...
            char filename[64];
            snprintf(filename, sizeof(filename), "%lu_pir_%d.jpg", timestamp, i + 1);
            
            // Upload to Supabase Storage
            bool uploaded = uploadToSupabase(fb->buf, fb->len, SUPABASE_BUCKET_IMAGES, filename);
            
            if (uploaded) {
                const char* imageUrl = getLastUploadedUrl();
                if (imageUrl && strlen(imageUrl) > 0) {
                    imageUrls[i] = String(imageUrl);
                    successCount++;
                    Serial.printf("[FEATURE] Burst %d uploaded: %s\n", i + 1, imageUrl);
                }
            } else {
                Serial.printf("[FEATURE] Failed to upload burst photo %d\n", i + 1);
            }
            
            releaseFrame(fb);
        } else {
            Serial.printf("[FEATURE] Failed to capture burst photo %d\n", i + 1);
        }
        
        // Delay between shots
        if (i < CAMERA_BURST_COUNT - 1) {
            delay(CAMERA_BURST_DELAY_MS);
        }
    }
    
    // Publish MQTT to doorbell/evt/pir_alert with array of image URLs (HIGH ALERT)
    if (successCount > 0) {
        char msg[1024];
        char urlsJson[800];
        urlsJson[0] = '\0';
        
        // Build JSON array of URLs: ["url1","url2","url3"]
        strcat(urlsJson, "[");
        for (int i = 0; i < CAMERA_BURST_COUNT; i++) {
            if (imageUrls[i].length() > 0) {
                if (strlen(urlsJson) > 1) strcat(urlsJson, ",");
                strcat(urlsJson, "\"");
                strcat(urlsJson, imageUrls[i].c_str());
                strcat(urlsJson, "\"");
            }
        }
        strcat(urlsJson, "]");
        
        snprintf(msg, sizeof(msg),
                 "{\"image_urls\":%s,\"count\":%d,\"timestamp\":%lu,\"level\":\"high\",\"description\":\"High alert burst images\"}",
                 urlsJson, successCount, timestamp);
        
        mqttPublishJson(TOPIC_EVT_PIR_ALERT, msg);
        Serial.printf("[FEATURE] Published to %s: %s\n", TOPIC_EVT_PIR_ALERT, msg);
    } else {
        Serial.println("[FEATURE] No photos uploaded in burst");
    }
}

void activateAlarm(int durationSeconds) {
    if (isAlarmActive) return;
    
    // Check if alarm is enabled in settings
    if (!getDeviceSettings().alarm_enabled) {
        Serial.println("[FEATURE] Alarm trigger ignored (disabled in settings)");
        return;
    }
    
    Serial.println("[FEATURE] Activating alarm");
    
    if (durationSeconds > 0) {
        Serial.printf("[FEATURE] Auto-off timer set for %d seconds\n", durationSeconds);
        alarmDuration = durationSeconds * 1000;
        alarmStartTime = millis();
    } else {
        alarmDuration = 0;
    }
    
    isAlarmActive = true;
    
    // Play alarm sound from SPIFFS
    playAudioFile(AUDIO_ALARM);
    // Alarm is triggered by PIR HIGH ALERT, no separate MQTT needed
}

void deactivateAlarm() {
    if (!isAlarmActive) return;
    
    Serial.println("[FEATURE] Deactivating alarm");
    isAlarmActive = false;
    alarmDuration = 0;
    
    // Stop audio if playing
    stopPlayback();
}

void handleAlarmTimer() {
    if (isAlarmActive && alarmDuration > 0) {
        if (millis() - alarmStartTime >= alarmDuration) {
            Serial.println("[FEATURE] Alarm auto-off timer expired");
            deactivateAlarm();
        }
    }
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
    // TODO: Implement two-way audio streaming (future feature)
}

void stopTwoWayAudio() {
    if (!isTwoWayAudioActive) return;
    
    Serial.println("[FEATURE] Stopping two-way audio");
    isTwoWayAudioActive = false;
    
    setWsAudioStreaming(false);
    stopWebStream();
    // Two-way audio stopped
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
    // Volume change is local only, controlled via TOPIC_CMD_SETTINGS
}

// --- Environment Features ---

void readEnvironmentTemperature() {
    float temp = readTemperatureCelsius();
    
    if (temp > -100.0f) { // Valid reading
        Serial.printf("[FEATURE] Environment temperature: %.1f°C\n", temp);
        
        // Publish to MQTT
        mqttPublishTemperature(temp);
        
        // Temperature published to TOPIC_SENSOR_TEMP via mqttPublishTemperature()
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
    
    // Flow 2.1: Publish normal motion detection to MQTT (for activity log)
    char msg[256];
    snprintf(msg, sizeof(msg),
             "{\"event\":\"motion_detected\",\"timestamp\":%lu,\"level\":\"normal\",\"device_id\":\"%s\",\"description\":\"PIR motion detected\"}",
             getTimestamp(),
             getDeviceId());
    mqttPublishJson(TOPIC_EVT_PIR, msg);
    Serial.printf("[PIR-REG] Published to %s: %s\n", TOPIC_EVT_PIR, msg);
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
            captureSecurityBurst();  // This will publish to TOPIC_EVT_PIR_ALERT with images
            activateAlarm();
            break;
            
        case ALERT_NORMAL:
            Serial.println("[PIR] Normal - Motion cleared");
            if (isAlarmActive) {
                deactivateAlarm();
            }
            // No MQTT needed for normal state
            break;
    }
}
