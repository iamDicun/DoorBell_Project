#include "audio_service.h"

#include "audio.h"
#include "config.h"

static bool voiceNoteRecording = false;

void audioServiceInit() {
    Serial.println("[AUDIO] service init (stub)");
}

void audioServiceLoop() {
    // placeholder for streaming buffers
}

void audioServicePlayChime() {
    Serial.println("[AUDIO] play chime request");
    playTestTone();
}

void audioServiceStartVoiceNote() {
    if (voiceNoteRecording) return;
    voiceNoteRecording = true;
    Serial.println("[AUDIO] start voice note recording (stub)");
    startRecording();
}

void audioServiceStopVoiceNote() {
    if (!voiceNoteRecording) return;
    voiceNoteRecording = false;
    Serial.println("[AUDIO] stop voice note recording (stub)");
    stopRecording();
}

void audioServiceHandleInboundPcm(const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    // TODO: stream inbound PCM to I2S speaker
}
