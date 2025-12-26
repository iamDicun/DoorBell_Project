#include <Arduino.h>
#include "doorbell_app.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    doorbellSetup();
}

void loop() {
    doorbellLoop();
    delay(5);
}
