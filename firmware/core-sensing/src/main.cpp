#include <Arduino.h>
#include "CaneController.h"

// Instantiate Master Cane Controller
static CaneController cane;

void setup() {
    // Initialize primary debug serial interface
    Serial.begin(115200);
    delay(500);

    // Boot safety subsystems
    cane.begin();
}

void loop() {
    // Non-blocking cooperative safety cycle
    cane.update();
}
