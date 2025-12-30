#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "config.h"

// --- Function Declarations ---
bool initMicrophone();
bool initSpeaker();

// Recording
void startRecording();
void stopRecording();
bool isRecordingActive();
bool isRecordingReady();
size_t getRecordedBytes();
int16_t* getRecordBuffer();

// Playback
void playRecording();
void playUploadedAudio();
void stopPlayback();
void playUrl(const char* url);
bool isPlayingActive();

// WebSocket streaming
void wsAudioStreamTask(void* param);
void setWsAudioStreaming(bool enabled);

// Upload buffer
uint8_t* getUploadBuffer();
void setUploadedBytes(size_t bytes);
void setUploadReady(bool ready);
size_t getUploadedBytes();

// Volume control
void setSpeakerVolume(float vol);
float getSpeakerVolume();

// Web to Speaker Streaming
void startWebStream();
void stopWebStream();
void processWebStream(uint8_t* payload, size_t length);

// Test tone
void playTestTone();

// Sound indicators for voice recording
void playRecordingStartTone();  // Middle tone
void playRecordingSuccessTone(); // High tone
void playRecordingErrorTone();   // Low tone

#endif
