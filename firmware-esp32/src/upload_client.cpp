#include "upload_client.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

static String lastUploadedUrl = "";

const char* getLastUploadedUrl() {
    return lastUploadedUrl.c_str();
}

void setLastUploadedUrl(const char* url) {
    lastUploadedUrl = String(url);
}

bool uploadToSupabase(const uint8_t* data, size_t len, const char* bucket, const char* filename) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SUPABASE] WiFi not connected");
        return false;
    }
    
    // Test DNS resolution before attempting upload
    IPAddress supabaseIP;
    const char* hostname = "xznnnklhqkccylxxzsdh.supabase.co";
    Serial.printf("[SUPABASE] Testing DNS resolution for %s...\n", hostname);
    
    if (!WiFi.hostByName(hostname, supabaseIP)) {
        Serial.println("[SUPABASE] ✗ DNS resolution FAILED!");
        Serial.println("[SUPABASE] Possible solutions:");
        Serial.println("  1. Check DNS server configuration");
        Serial.println("  2. Verify internet connectivity");
        Serial.println("  3. Check if hostname is correct");
        Serial.printf("[SUPABASE] Current DNS: %s\n", WiFi.dnsIP().toString().c_str());
        return false;
    }
    
    Serial.printf("[SUPABASE] ✓ DNS resolved to: %s\n", supabaseIP.toString().c_str());
    
    // Build Supabase Storage API URL
    String url = String(SUPABASE_URL) + "/storage/v1/object/" + bucket + "/" + filename;
    Serial.printf("[SUPABASE] Uploading to: %s (%u bytes)\n", url.c_str(), (unsigned)len);
    
    // Small delay to allow DNS to settle
    delay(100);
    
    // Create new WiFiClientSecure for each request to avoid DNS cache issues
    WiFiClientSecure* uploadSecureClient = new WiFiClientSecure();
    uploadSecureClient->setInsecure(); // Skip certificate validation
    uploadSecureClient->setTimeout(30); // 30 second timeout
    
    Serial.println("[SUPABASE] Connecting to server...");
    
    HTTPClient http;
    
    // Try to begin connection with retry
    bool connected = false;
    for (int i = 0; i < 3; i++) {
        if (http.begin(*uploadSecureClient, url)) {
            connected = true;
            Serial.println("[SUPABASE] ✓ HTTP connection established");
            break;
        }
        Serial.printf("[SUPABASE] Connection attempt %d failed, retrying...\n", i + 1);
        delay(1000);
    }
    
    if (!connected) {
        Serial.println("[SUPABASE] Failed to begin HTTP connection after 3 attempts");
        uploadSecureClient->stop();
        delete uploadSecureClient;
        return false;
    }
    
    // Set required headers for Supabase Storage API
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("x-upsert", "true"); // Overwrite if exists
    http.setTimeout(30000); // 30 second timeout
    
    int httpCode = http.POST((uint8_t*)data, len);
    
    bool success = false;
    String publicUrl = "";
    
    if (httpCode > 0) {
        Serial.printf("[SUPABASE] Response code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            Serial.printf("[SUPABASE] Response: %s\n", response.c_str());
            
            // Build public URL
            publicUrl = String(SUPABASE_URL) + "/storage/v1/object/public/" + bucket + "/" + filename;
            Serial.printf("[SUPABASE] Public URL: %s\n", publicUrl.c_str());
            
            setLastUploadedUrl(publicUrl.c_str());
            success = true;
        } else {
            Serial.printf("[SUPABASE] HTTP error: %d\n", httpCode);
            String response = http.getString();
            Serial.printf("[SUPABASE] Error response: %s\n", response.c_str());
        }
    } else {
        Serial.printf("[SUPABASE] Request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    uploadSecureClient->stop();
    delay(100); // Allow cleanup
    delete uploadSecureClient;
    return success;
}

bool uploadClientPost(const char* endpoint, const uint8_t* data, size_t len, const char* contentType, 
                      const char* eventType, unsigned long timestamp) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[UPLOAD] WiFi not connected");
        return false;
    }
    
    // Construct full URL
    String url = String(BACKEND_BASE_URL) + endpoint;
    Serial.printf("[UPLOAD] POST %s (%u bytes, %s)\n", url.c_str(), (unsigned)len, contentType);
    
    // Create new WiFiClientSecure for each request
    WiFiClientSecure* client = new WiFiClientSecure();
    client->setInsecure(); // Skip certificate validation
    client->setTimeout(30); // 30 second timeout
    
    delay(100); // Allow WiFi to stabilize
    
    HTTPClient http;
    
    if (!http.begin(*client, url)) {
        Serial.println("[UPLOAD] HTTP begin failed");
        client->stop();
        delete client;
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
    String uploadedUrl = "";
    
    if (httpCode > 0) {
        Serial.printf("[UPLOAD] Response code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_ACCEPTED) {
            String response = http.getString();
            Serial.printf("[UPLOAD] Response: %s\n", response.c_str());
            
            // Parse JSON response to get URL
            // Expected: {"success":true,"url":"https://...supabase.co/..."}
            int urlStart = response.indexOf("\"url\":\"");
            if (urlStart != -1) {
                urlStart += 7; // Skip "url":"
                int urlEnd = response.indexOf("\"", urlStart);
                if (urlEnd != -1) {
                    uploadedUrl = response.substring(urlStart, urlEnd);
                    Serial.printf("[UPLOAD] Extracted URL: %s\n", uploadedUrl.c_str());
                }
            }
            
            success = true;
        } else {
            Serial.printf("[UPLOAD] HTTP error: %d\n", httpCode);
        }
    } else {
        Serial.printf("[UPLOAD] Request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    client->stop();
    delay(100); // Allow cleanup
    delete client;
    
    // Store URL for caller to retrieve
    if (success && uploadedUrl.length() > 0) {
        setLastUploadedUrl(uploadedUrl.c_str());
    }
    
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
