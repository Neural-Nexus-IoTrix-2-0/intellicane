/**
 * ============================================================================
 * Intelligent Cane — Hardware Diagnostic & Robust Driver (ESP32-C3 SuperMini)
 * Neural-Nexus | IoTrix 2.0 (Track A Embedded IoT)
 * 
 * Hardware Connections:
 *   - HC-SR04:   TRIG -> GPIO 0, ECHO -> GPIO 1 (via divider), VCC -> 5V, GND -> GND
 *   - Motor:     Base/Gate -> GPIO 6 (Transistor driver), VCC -> 3V3/5V, GND -> GND
 *   - Buzzer:    (+) -> GPIO 7, (-) -> GND (Supports BOTH Active & Passive buzzers)
 *   - Button:    Pin 1 -> GPIO 3, Pin 2 -> GND (INPUT_PULLUP)
 *   - MPU6050:   SDA -> GPIO 4, SCL -> GPIO 5, VCC -> 3V3, GND -> GND
 *   - LED:       Onboard Blue LED -> GPIO 8 (Active LOW)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ============================================================================
// Pin Definitions (ESP32-C3 SuperMini)
// ============================================================================
constexpr uint8_t PIN_US_TRIG    = 0; // Ultrasonic Trigger
constexpr uint8_t PIN_US_ECHO    = 1; // Ultrasonic Echo
constexpr uint8_t PIN_VIBRATION  = 6; // Vibration Motor
constexpr uint8_t PIN_BUZZER     = 7; // Piezo Buzzer (Active or Passive)
constexpr uint8_t PIN_BUTTON     = 3; // Pushbutton
constexpr uint8_t PIN_LED_C3     = 8; // Onboard LED (Active LOW)

constexpr uint8_t PIN_I2C_SDA    = 4;
constexpr uint8_t PIN_I2C_SCL    = 5;

Adafruit_MPU6050 mpu;
bool mpuAvailable = false;

float distanceCm = 200.0f;
float tiltAngleDeg = 0.0f;
float gMagnitude = 1.0f;
bool fallAlert = false;
uint8_t currentMotorPwm = 0;

uint32_t lastSensorMs = 0;
uint32_t lastTelemMs  = 0;
uint32_t lastBuzzerMs = 0;
bool buzzerState = false;

// ============================================================================
// Universal Buzzer Driver (Works on BOTH Active and Passive Buzzers)
// ============================================================================
void setBuzzer(bool state) {
    if (state) {
        // DC HIGH triggers Active Buzzers; also generates tone for Passive
        digitalWrite(PIN_BUZZER, HIGH);
    } else {
        digitalWrite(PIN_BUZZER, LOW);
    }
}

// Generate sound pulse (duration in ms)
void beepBuzzer(uint16_t durationMs) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(durationMs);
    digitalWrite(PIN_BUZZER, LOW);
}

// Measure HC-SR04 distance
float readDistanceCm() {
    digitalWrite(PIN_US_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    unsigned long durationUs = pulseIn(PIN_US_ECHO, HIGH, 26000);
    if (durationUs == 0) return 400.0f;
    return (float)durationUs * 0.0343f / 2.0f;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("  INTELLIGENT CANE — HARDWARE TEST & FIRMWARE (C3)");
    Serial.println("=======================================================");

    // Pin configurations
    pinMode(PIN_US_TRIG, OUTPUT);
    pinMode(PIN_US_ECHO, INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED_C3, OUTPUT);

    digitalWrite(PIN_US_TRIG, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_C3, HIGH); // OFF

    // Initialize Motor PWM at 200 Hz (Optimal for DC brushed vibration motors)
    ledcAttach(PIN_VIBRATION, 200, 8);
    ledcWrite(PIN_VIBRATION, 0);

    // ========================================================================
    // STARTUP SELF-TEST: Confirms Buzzer, Motor, and LED immediately
    // ========================================================================
    Serial.println("[Self-Test] 1. Testing Buzzer (2 Beeps)...");
    beepBuzzer(150);
    delay(100);
    beepBuzzer(150);

    Serial.println("[Self-Test] 2. Testing Vibration Motor (1-Second Full Spin)...");
    digitalWrite(PIN_LED_C3, LOW); // LED ON
    ledcWrite(PIN_VIBRATION, 255); // 100% full spin
    delay(1000);
    ledcWrite(PIN_VIBRATION, 0);   // Motor OFF
    digitalWrite(PIN_LED_C3, HIGH); // LED OFF

    // Initialize I2C and MPU6050
    Serial.println("[Self-Test] 3. Initializing MPU6050 (SDA=4, SCL=5)...");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (mpu.begin(0x68, &Wire)) {
        mpuAvailable = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        Serial.println("[Self-Test] MPU6050 connected successfully!");
    } else {
        Serial.println("[Self-Test] WARNING: MPU6050 not detected. Continuing...");
    }

    Serial.println("\n[System] Self-test complete. Running live obstacle loop...\n");
}

void loop() {
    uint32_t now = millis();

    // ------------------------------------------------------------------------
    // 1. Read Sensors every 40ms
    // ------------------------------------------------------------------------
    if (now - lastSensorMs >= 40) {
        lastSensorMs = now;
        distanceCm = readDistanceCm();

        // Pushbutton: resets fall alarm
        if (digitalRead(PIN_BUTTON) == LOW && fallAlert) {
            fallAlert = false;
            Serial.println("[Button] Alarm cleared.");
        }

        // MPU6050 Read
        if (mpuAvailable) {
            sensors_event_t a, g, temp;
            if (mpu.getEvent(&a, &g, &temp)) {
                float rawMag = sqrtf(a.acceleration.x * a.acceleration.x +
                                     a.acceleration.y * a.acceleration.y +
                                     a.acceleration.z * a.acceleration.z);
                gMagnitude = rawMag / 9.80665f;

                if (rawMag > 0.1f) {
                    float cosTilt = fabsf(a.acceleration.z) / rawMag;
                    if (cosTilt > 1.0f) cosTilt = 1.0f;
                    tiltAngleDeg = acosf(cosTilt) * 180.0f / (float)M_PI;
                }

                if (tiltAngleDeg >= 65.0f || gMagnitude >= 2.5f) {
                    fallAlert = true;
                } else if (tiltAngleDeg < 30.0f) {
                    fallAlert = false;
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // 2. Vibration Motor Intensity Mapping
    // ------------------------------------------------------------------------
    // > 100cm: Clear (OFF)
    // 40cm - 100cm: Proportional (PWM 100 to 255)
    // < 40cm: Danger (100% full vibration)
    if (fallAlert) {
        // Fall alert: Rhythmic burst 200ms ON / 200ms OFF
        currentMotorPwm = (now % 400 < 200) ? 255 : 0;
    } else if (distanceCm > 120.0f) {
        currentMotorPwm = 0; // Clear zone: OFF
    } else if (distanceCm <= 40.0f) {
        currentMotorPwm = 255; // Close hazard: FULL 100%
    } else {
        // Linear ramp from 120cm (PWM 100) down to 40cm (PWM 255)
        float progress = (120.0f - distanceCm) / (120.0f - 40.0f);
        currentMotorPwm = (uint8_t)(100 + progress * (255 - 100));
    }

    ledcWrite(PIN_VIBRATION, currentMotorPwm);

    // ------------------------------------------------------------------------
    // 3. Buzzer Alarm & Status LED
    // ------------------------------------------------------------------------
    if (fallAlert) {
        // Alternating siren for fall
        if (now - lastBuzzerMs >= 200) {
            lastBuzzerMs = now;
            buzzerState = !buzzerState;
            setBuzzer(buzzerState);
            digitalWrite(PIN_LED_C3, buzzerState ? LOW : HIGH);
        }
    } else if (distanceCm <= 30.0f) {
        // Urgent beeping for close obstacle (< 30cm)
        if (now - lastBuzzerMs >= 100) {
            lastBuzzerMs = now;
            buzzerState = !buzzerState;
            setBuzzer(buzzerState);
            digitalWrite(PIN_LED_C3, buzzerState ? LOW : HIGH);
        }
    } else {
        setBuzzer(false);
        // Visual indicator of motor activity
        digitalWrite(PIN_LED_C3, (currentMotorPwm > 0) ? LOW : HIGH);
    }

    // ------------------------------------------------------------------------
    // 4. Telemetry Stream every 250ms
    // ------------------------------------------------------------------------
    if (now - lastTelemMs >= 250) {
        lastTelemMs = now;

        const char* alertMsg = "CLEAR";
        if (fallAlert) alertMsg = "FALL ALARM!";
        else if (distanceCm <= 30.0f) alertMsg = "CRITICAL HAZARD!";
        else if (distanceCm <= 70.0f) alertMsg = "WARNING";
        else if (distanceCm <= 120.0f) alertMsg = "CAUTION";

        uint8_t vibPercent = (uint8_t)((currentMotorPwm / 255.0f) * 100.0f);

        Serial.printf("[CANE-C3] Dist: %5.1f cm | Tilt: %4.1f° | Vib: %3d%% (PWM: %3d) | Buzzer: %s | Alert: %s\n",
                      distanceCm,
                      tiltAngleDeg,
                      vibPercent,
                      currentMotorPwm,
                      (distanceCm <= 30.0f || fallAlert) ? "BEEPING" : "MUTED",
                      alertMsg);
    }
}
