#pragma once

#include <Arduino.h>

bool uploadClientPost(const char* endpoint, const uint8_t* data, size_t len, const char* contentType);
bool uploadWithRetry(const char* endpoint, const uint8_t* data, size_t len, const char* contentType, int maxRetries = 3);
