/**
 * ============================================================================
 * Intelligent Cane — Phase 1 Wokwi Simulation
 * Neural-Nexus | IoTrix 2.0 (Track A Embedded IoT)
 * ============================================================================
 * 
 * Hardware Connections in this Simulation:
 *   - HC-SR04 (Ultrasonic):   VCC->5V, GND->GND, TRIG->GPIO 18, ECHO->GPIO 5
 *   - MPU6050 (6-Axis IMU):   VCC->3V3, GND->GND, SCL->GPIO 22, SDA->GPIO 21
 *   - Piezo Buzzer:           Pin 1(-)->GND, Pin 2(+)->GPIO 26
 *   - Green Pushbutton:       Pin 1.l->GND, Pin 2.r->GPIO 27 (INPUT_PULLUP)
 *   - IR Receiver:            VCC->3V3, GND->GND, DAT->GPIO 13
 *   - Onboard Status LED:     GPIO 2 (Indicates simulated haptic vibration)
 * 
 * Interactive Controls in Wokwi:
 *   1. Click HC-SR04 to slide obstacle distance.
 *   2. Click MPU6050 to change tilt / simulate a fall.
 *   3. Click IR Receiver -> Click "Send" or send an NEC command.
 *   4. Click Green Button to reset alarms / trigger SOS.
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <IRremote.hpp>

// ============================================================================
// Pin Definitions
// ============================================================================
constexpr uint8_t PIN_US_TRIG   = 18; // Ultrasonic Trigger
constexpr uint8_t PIN_US_ECHO   = 5;  // Ultrasonic Echo
constexpr uint8_t PIN_BUZZER    = 26; // Piezo Buzzer
constexpr uint8_t PIN_BUTTON    = 27; // SOS / Reset Pushbutton (Active LOW)
constexpr uint8_t PIN_IR_RECV   = 13; // IR Receiver DAT Pin
constexpr uint8_t PIN_LED_STATE = 2;  // Built-in LED (simulates haptic vibration)

// ============================================================================
// Safety Thresholds
// ============================================================================
// Dynamic threshold depending on mode (Outdoor vs. Indoor)
float distClearCm    = 150.0f; // > 150cm = Path clear
float distCautionCm  = 60.0f;  // 60-150cm = Caution
float distWarningCm  = 25.0f;  // 25-60cm = Warning
constexpr float DIST_CRITICAL_CM = 15.0f; // < 15cm = Critical proximity hazard

constexpr float TILT_FALL_DEG    = 65.0f; // Cane horizontal on ground
constexpr float IMPACT_SPIKE_G   = 2.5f;  // Sudden drop impact

// Operating Modes
enum CaneMode {
    MODE_OUTDOOR, // Standard 150cm range
    MODE_INDOOR   // Compact 80cm range for tight spaces & doorways
};
CaneMode caneMode = MODE_OUTDOOR;

// ============================================================================
// Objects & Global State
// ============================================================================
Adafruit_MPU6050 mpu;
bool mpuAvailable = false;

// Sensor Readings
float distanceCm = 200.0f;
float tiltAngleDeg = 0.0f;
float gMagnitude = 1.0f;
bool buttonPressed = false;

// Safety State
bool fallAlert = false;
bool criticalObstacle = false;
bool findMeMelodyActive = false;
uint32_t findMeStartMs = 0;
uint8_t simulatedVibrationPwm = 0; // 0 to 255

// Cooperative timing intervals
uint32_t lastSensorReadMs   = 0;
uint32_t lastTelemetryMs    = 0;
uint32_t lastBuzzerToggleMs = 0;
bool buzzerState = false;

// ============================================================================
// Helper: Ultrasonic Distance Reading (Non-blocking trigger)
// ============================================================================
float readUltrasonicDistanceCm() {
    digitalWrite(PIN_US_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    unsigned long duration = pulseIn(PIN_US_ECHO, HIGH, 26000); // ~4.5m timeout
    if (duration == 0) {
        return 400.0f; // Out of range / clear
    }
    return (float)duration * 0.0343f / 2.0f;
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(400);

    Serial.println("\n=======================================================");
    Serial.println("  INTELLIGENT CANE — WOKWI SIMULATION");
    Serial.println("  Neural-Nexus | IoTrix 2.0 (Track A Embedded IoT)");
    Serial.println("=======================================================");

    // Pin configurations
    pinMode(PIN_US_TRIG, OUTPUT);
    pinMode(PIN_US_ECHO, INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED_STATE, OUTPUT);

    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_STATE, LOW);

    // Initialize I2C and MPU6050
    Serial.println("[System] Initializing I2C (SDA=21, SCL=22)...");
    Wire.begin(21, 22);

    if (mpu.begin(0x68, &Wire)) {
        mpuAvailable = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println("[System] MPU6050 6-Axis IMU connected successfully!");
    } else {
        Serial.println("[System] WARNING: MPU6050 not found on 0x68. Continuing...");
    }

    // Initialize IR Receiver on GPIO 13
    IrReceiver.begin(PIN_IR_RECV, DISABLE_LED_FEEDBACK);
    Serial.printf("[System] IR Receiver initialized on GPIO %d\n", PIN_IR_RECV);

    // Confirmation boot pip
    tone(PIN_BUZZER, 2000, 150);
    digitalWrite(PIN_LED_STATE, HIGH);
    delay(150);
    digitalWrite(PIN_LED_STATE, LOW);

    Serial.println("-------------------------------------------------------");
    Serial.println("  INTERACTIVE TIPS IN WOKWI:");
    Serial.println("  - Click HC-SR04 to slide obstacle distance (cm)");
    Serial.println("  - Click MPU6050 to change tilt / simulate a fall");
    Serial.println("  - Click IR Receiver -> Click 'Send' to test remote command");
    Serial.println("  - Press Green Button to mute/reset alert");
    Serial.println("=======================================================\n");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    uint32_t now = millis();

    // ------------------------------------------------------------------------
    // 1. Process Incoming IR Remote Commands
    // ------------------------------------------------------------------------
    if (IrReceiver.decode()) {
        uint32_t cmd = IrReceiver.decodedIRData.command;
        Serial.printf("\n[IR Remote] Signal Received! Command: 0x%02X (Protocol: %s)\n",
                      cmd, getProtocolString(IrReceiver.decodedIRData.protocol));

        // Feedback beep on receipt
        tone(PIN_BUZZER, 1800, 80);

        // Feature 1: Remotely silence active alarms
        if (fallAlert) {
            fallAlert = false;
            Serial.println("[IR Remote] Action -> Fall alarm silenced remotely.");
        } 
        // Feature 2: Trigger "Find My Cane" audio beacon
        else {
            findMeMelodyActive = true;
            findMeStartMs = now;
            // Toggle indoor/outdoor mode on IR signal
            if (caneMode == MODE_OUTDOOR) {
                caneMode = MODE_INDOOR;
                distClearCm   = 80.0f;
                distCautionCm = 45.0f;
                distWarningCm = 20.0f;
                Serial.println("[Mode] Switched to INDOOR MODE (Range: 80cm)");
            } else {
                caneMode = MODE_OUTDOOR;
                distClearCm   = 150.0f;
                distCautionCm = 60.0f;
                distWarningCm = 25.0f;
                Serial.println("[Mode] Switched to OUTDOOR MODE (Range: 150cm)");
            }
        }

        IrReceiver.resume(); // Ready for next signal
    }

    // ------------------------------------------------------------------------
    // 2. Read Sensors every 50ms
    // ------------------------------------------------------------------------
    if (now - lastSensorReadMs >= 50) {
        lastSensorReadMs = now;

        // Read Distance
        distanceCm = readUltrasonicDistanceCm();

        // Read Pushbutton (Active LOW)
        buttonPressed = (digitalRead(PIN_BUTTON) == LOW);
        if (buttonPressed && fallAlert) {
            fallAlert = false;
            Serial.println("[User] Button pressed -> Fall alarm cleared.");
        }

        // Read MPU6050
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

                // Fall detection logic: horizontal cane (>65 deg) or hard impact (>2.5g)
                if (tiltAngleDeg >= TILT_FALL_DEG || gMagnitude >= IMPACT_SPIKE_G) {
                    fallAlert = true;
                } else if (!buttonPressed && tiltAngleDeg < 30.0f) {
                    // Cane returned upright
                    fallAlert = false;
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // 3. Safety Arbitration & Simulated Haptics
    // ------------------------------------------------------------------------
    criticalObstacle = (distanceCm <= DIST_CRITICAL_CM);

    if (distanceCm > distClearCm) {
        simulatedVibrationPwm = 0; // Clear
    } else if (distanceCm <= DIST_CRITICAL_CM) {
        simulatedVibrationPwm = 255; // 100% full vibration
    } else {
        float norm = (distClearCm - distanceCm) / (distClearCm - DIST_CRITICAL_CM);
        simulatedVibrationPwm = (uint8_t)(80 + norm * (255 - 80));
    }

    // ------------------------------------------------------------------------
    // 4. Audio Alarm & Visual LED Patterns
    // ------------------------------------------------------------------------
    if (findMeMelodyActive) {
        // "Find My Cane" cheerful chirp melody for 800ms
        uint32_t elapsed = now - findMeStartMs;
        if (elapsed < 200)       tone(PIN_BUZZER, 1318); // E6
        else if (elapsed < 400)  tone(PIN_BUZZER, 1568); // G6
        else if (elapsed < 600)  tone(PIN_BUZZER, 2093); // C7
        else {
            noTone(PIN_BUZZER);
            findMeMelodyActive = false;
        }
    } else if (fallAlert) {
        // High-priority alternating siren
        if (now - lastBuzzerToggleMs >= 150) {
            lastBuzzerToggleMs = now;
            buzzerState = !buzzerState;
            if (buzzerState) {
                tone(PIN_BUZZER, 2400);
                digitalWrite(PIN_LED_STATE, HIGH);
            } else {
                tone(PIN_BUZZER, 1200);
                digitalWrite(PIN_LED_STATE, LOW);
            }
        }
    } else if (criticalObstacle) {
        // Urgent rapid beeping: 80ms ON, 80ms OFF (< 15cm obstacle)
        if (now - lastBuzzerToggleMs >= 80) {
            lastBuzzerToggleMs = now;
            buzzerState = !buzzerState;
            if (buzzerState) {
                tone(PIN_BUZZER, 2800);
                digitalWrite(PIN_LED_STATE, HIGH);
            } else {
                noTone(PIN_BUZZER);
                digitalWrite(PIN_LED_STATE, LOW);
            }
        }
    } else if (distanceCm <= distWarningCm) {
        // Warning zone: Intermittent chirp
        if (now - lastBuzzerToggleMs >= 250) {
            lastBuzzerToggleMs = now;
            buzzerState = !buzzerState;
            if (buzzerState) {
                tone(PIN_BUZZER, 1800, 100);
                digitalWrite(PIN_LED_STATE, HIGH);
            } else {
                digitalWrite(PIN_LED_STATE, LOW);
            }
        }
    } else {
        // Safe clear path
        noTone(PIN_BUZZER);
        digitalWrite(PIN_LED_STATE, (simulatedVibrationPwm > 100 && (now % 300 < 50)) ? HIGH : LOW);
    }

    // ------------------------------------------------------------------------
    // 5. Formatted Serial Telemetry every 250ms
    // ------------------------------------------------------------------------
    if (now - lastTelemetryMs >= 250) {
        lastTelemetryMs = now;

        const char* statusStr = "CLEAR";
        if (fallAlert)             statusStr = "FALL ALARM!";
        else if (criticalObstacle) statusStr = "CRITICAL HAZARD!";
        else if (distanceCm <= distWarningCm) statusStr = "WARNING";
        else if (distanceCm <= distCautionCm) statusStr = "CAUTION";

        uint8_t vibPercent = (uint8_t)((simulatedVibrationPwm / 255.0f) * 100.0f);
        const char* modeStr = (caneMode == MODE_OUTDOOR) ? "OUTDOOR" : "INDOOR";

        Serial.printf("[CANE] Mode: %-7s | Dist: %5.1f cm | Tilt: %4.1f deg | G: %4.2f | Vib: %3d%% | Alert: %-16s\n",
                      modeStr,
                      distanceCm,
                      tiltAngleDeg,
                      gMagnitude,
                      vibPercent,
                      statusStr);
    }
}
