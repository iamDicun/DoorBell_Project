#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <Arduino.h>
#include "esp_camera.h"
#include "config.h"

bool initCamera();
camera_fb_t* captureFrame();
void releaseFrame(camera_fb_t* fb);

#endif
