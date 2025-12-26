#include "camera_service.h"

#include <Arduino.h>
#include "camera_module.h"

void cameraServiceInit() {
    Serial.println("[CAM] service init (stub)");
}

void cameraServiceLoop() {
    // Placeholder for streaming endpoint handling
}

void cameraServiceCaptureBurst() {
    Serial.println("[CAM] capture burst (stub)");
    camera_fb_t* fb = captureFrame();
    if (fb) {
        Serial.printf("[CAM] captured frame len=%u\n", fb->len);
        releaseFrame(fb);
    }
}
