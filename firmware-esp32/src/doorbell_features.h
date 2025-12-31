#pragma once

#include <Arduino.h>

// --- Button & Door Features ---
void playDingDong();
void playDingDong2();
void playDingDong3();
void startVoiceNoteRecording();
void stopVoiceNoteRecording();

// --- Camera & Security Features ---
void captureGuestPhoto();       // Button press -> doorbell/evt/button -> _button.jpg
void captureSnapshotPhoto();    // Manual snapshot -> doorbell/evt/snapshot -> _snapshot.jpg
void captureSecurityBurst();    // PIR burst -> doorbell/evt/pir_alert -> _pir_X.jpg
void activateAlarm(int durationSeconds = 0);
void deactivateAlarm();
void handleAlarmTimer();

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
    ALERT_HIGH = 1
};

PIRAlertLevel checkPIRAlertLevel();
void handlePIRAlert(PIRAlertLevel level);
void registerPIRDetection(unsigned long timestamp);
