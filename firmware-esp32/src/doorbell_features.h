#pragma once

#include <Arduino.h>

// --- Button & Door Features ---
void playDingDong();
void playDingDong2();
void playDingDong3();
void startVoiceNoteRecording();
void stopVoiceNoteRecording();

// --- Camera & Security Features ---
void captureGuestPhoto();
void captureSecurityBurst();
void activateAlarm();
void deactivateAlarm();

// --- Audio & Speaker Features ---
void startTwoWayAudio();
void stopTwoWayAudio();
void playSampleMessage(const char* messageType);
void setVolumeLevel(float level);

// --- Environment Features ---
void readEnvironmentTemperature();

// --- PIR Alert System ---
enum PIRAlertLevel {
    ALERT_NORMAL = 0,
    ALERT_MEDIUM = 1,
    ALERT_HIGH = 2
};

PIRAlertLevel checkPIRAlertLevel();
void handlePIRAlert(PIRAlertLevel level);
void registerPIRDetection(unsigned long timestamp);
