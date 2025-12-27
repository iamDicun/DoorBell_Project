#pragma once

#include <Arduino.h>

bool uploadClientPost(const char* endpoint, const uint8_t* data, size_t len, const char* contentType, 
                      const char* eventType = nullptr, unsigned long timestamp = 0);
bool uploadWithRetry(const char* endpoint, const uint8_t* data, size_t len, const char* contentType, 
                     int maxRetries = 3, const char* eventType = nullptr, unsigned long timestamp = 0);
