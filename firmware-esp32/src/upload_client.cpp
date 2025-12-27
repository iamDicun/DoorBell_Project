#include "upload_client.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

static WiFiClientSecure uploadSecureClient;

bool uploadClientPost(const char* endpoint, const uint8_t* data, size_t len, const char* contentType, 
                      const char* eventType, unsigned long timestamp) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[UPLOAD] WiFi not connected");
        return false;
    }
    
    // Construct full URL
    String url = String(BACKEND_BASE_URL) + endpoint;
    Serial.printf("[UPLOAD] POST %s (%u bytes, %s)\n", url.c_str(), (unsigned)len, contentType);
    
    HTTPClient http;
    uploadSecureClient.setInsecure(); // For production, use CA cert
    
    if (!http.begin(uploadSecureClient, url)) {
        Serial.println("[UPLOAD] HTTP begin failed");
        return false;
    }
    
    http.addHeader("Content-Type", contentType);
    
    // Add custom headers for metadata
    if (eventType != nullptr) {
        http.addHeader("X-Event-Type", eventType);
    }
    if (timestamp > 0) {
        char tsStr[32];
        snprintf(tsStr, sizeof(tsStr), "%lu", timestamp);
        http.addHeader("X-Timestamp", tsStr);
    }
    
    http.setTimeout(30000); // 30 second timeout
    
    int httpCode = http.POST((uint8_t*)data, len);
    
    bool success = false;
    if (httpCode > 0) {
        Serial.printf("[UPLOAD] Response code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_ACCEPTED) {
            String response = http.getString();
            Serial.printf("[UPLOAD] Response: %s\n", response.c_str());
            success = true;
        } else {
            Serial.printf("[UPLOAD] HTTP error: %d\n", httpCode);
        }
    } else {
        Serial.printf("[UPLOAD] Request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return success;
}

bool uploadWithRetry(const char* endpoint, const uint8_t* data, size_t len, const char* contentType, 
                     int maxRetries, const char* eventType, unsigned long timestamp) {
    Serial.printf("[UPLOAD] Retry upload (max %d attempts)\n", maxRetries);
    
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("[UPLOAD] Attempt %d/%d\n", attempt, maxRetries);
        
        if (uploadClientPost(endpoint, data, len, contentType, eventType, timestamp)) {
            Serial.println("[UPLOAD] Success!");
            return true;
        }
        
        if (attempt < maxRetries) {
            // Exponential backoff: 1s, 2s, 4s, 8s...
            unsigned long delayMs = 1000 * (1 << (attempt - 1));
            delayMs = min(delayMs, 10000UL); // Cap at 10 seconds
            Serial.printf("[UPLOAD] Waiting %lu ms before retry...\n", delayMs);
            delay(delayMs);
        }
    }
    
    Serial.println("[UPLOAD] All retry attempts failed");
    return false;
}
