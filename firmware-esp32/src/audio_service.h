#pragma once

#include <Arduino.h>

void audioServiceInit();
void audioServiceLoop();
void audioServicePlayChime();
void audioServiceStartVoiceNote();
void audioServiceStopVoiceNote();
void audioServiceHandleInboundPcm(const uint8_t* data, size_t len);
