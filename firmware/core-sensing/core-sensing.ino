/**
 * ============================================================================
 * Intelligent Cane — Hardware Diagnostic & Robust Driver (ESP32-C3 SuperMini)
 * Neural-Nexus | IoTrix 2.0 (Track A Embedded IoT)
 * 
 * Hardware Connections (ESP32-C3 SuperMini):
 *   - HC-SR04:       TRIG -> GPIO 0, ECHO -> GPIO 1, VCC -> 5V, GND -> GND
 *   - 3-Pin Motor:   IN/SIG -> GPIO 6, VCC -> 5V, GND -> GND (Integrated driver)
 *   - Buzzer:        (+) -> GPIO 7, (-) -> GND (Supports BOTH Active & Passive buzzers)
 *   - Button:        GPIO 3 (Optional / Unconnected in bench setup; pullup active)
 *   - MPU6050:       SDA -> GPIO 4, SCL -> GPIO 5, VCC -> 5V, GND -> GND
 *   - Status LED:    Onboard Blue LED -> GPIO 8 (Active LOW)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <esp_arduino_version.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ProximityFeedback.h"

// BLE UUIDs for Intelligent Cane Telemetry Service
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool bleClientConnected = false;
BLEServer *pBleServer = nullptr;
BLECharacteristic *pTelemetryCharacteristic = nullptr;

class CaneBLECallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        bleClientConnected = true;
        Serial.println("\n=======================================================");
        Serial.printf("  [BLE] >>> CLIENT CONNECTED! (Active: %u) <<<\n", pServer->getConnectedCount());
        Serial.println("=======================================================\n");
    }

    void onDisconnect(BLEServer* pServer) override {
        bleClientConnected = false;
        Serial.println("\n=======================================================");
        Serial.println("  [BLE] <<< CLIENT DISCONNECTED! Restarting advertising... >>>");
        Serial.println("=======================================================\n");
        BLEDevice::startAdvertising();
    }
};

void initBLE() {
    Serial.println("[BLE] Initializing Bluetooth Low Energy (BLE)...");
    BLEDevice::init("Intelligent-Cane");

    String localMac = BLEDevice::getAddress().toString();
    Serial.printf("[BLE] Device Name : Intelligent-Cane\n");
    Serial.printf("[BLE] Hardware MAC: %s\n", localMac.c_str());

    pBleServer = BLEDevice::createServer();
    pBleServer->setCallbacks(new CaneBLECallbacks());

    BLEService *pService = pBleServer->createService(BLE_SERVICE_UUID);
    pTelemetryCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTelemetryCharacteristic->addDescriptor(new BLE2902());
    pTelemetryCharacteristic->setValue("Cane Online");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Advertising started. Discoverable as 'Intelligent-Cane'.");
}

// ============================================================================
// Pin Definitions (ESP32-C3 SuperMini)
// ============================================================================
constexpr uint8_t PIN_US_TRIG    = 0; // Ultrasonic Trigger
constexpr uint8_t PIN_US_ECHO    = 1; // Ultrasonic Echo
constexpr uint8_t PIN_VIBRATION  = 6; // Vibration Motor (3-Pin Module IN)
constexpr uint8_t PIN_BUZZER     = 7; // Piezo Buzzer (Active or Passive)
constexpr uint8_t PIN_BUTTON     = 3; // Pushbutton (Optional - unpopulated on bench build)
constexpr uint8_t PIN_LED_C3     = 8; // Onboard LED (Active LOW)

constexpr uint8_t PIN_I2C_SDA    = 4;
constexpr uint8_t PIN_I2C_SCL    = 5;

// Motor Polarity Configuration
// Set to true if your 3-pin vibration module is Active-LOW (vibrates when input is LOW)
constexpr bool MOTOR_ACTIVE_LOW  = false;
// Active buzzer: DC on/off. Passive piezo: dedicated audio-frequency PWM.
constexpr bool BUZZER_IS_PASSIVE = false;
constexpr uint8_t MOTOR_CHANNEL = 0;
constexpr uint8_t BUZZER_CHANNEL = 2; // Separate timer from motor on ESP32-C3.
BeepEnvelope beepEnvelope;

Adafruit_MPU6050 mpu;
bool mpuAvailable = false;

float distanceCm = NAN;
float tiltAngleDeg = 0.0f;
float gMagnitude = 1.0f;
bool fallAlert = false;
uint8_t currentMotorPwm = 0;

uint32_t lastSensorMs = 0;
uint32_t lastTelemMs  = 0;
bool buzzerState = false;

void setMotorPwm(uint8_t duty) {
    uint8_t output = motorOutputDuty(duty, MOTOR_ACTIVE_LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_VIBRATION, output);
#else
    ledcWrite(MOTOR_CHANNEL, output);
#endif
}

void setBuzzer(bool state, uint16_t frequencyHz = 2000) {
    static bool initialized = false;
    static bool previousState = false;
    static uint16_t previousFrequency = 0;
    if (initialized && state == previousState &&
        (!BUZZER_IS_PASSIVE || !state || frequencyHz == previousFrequency)) return;
    initialized = true;
    previousState = state;
    previousFrequency = frequencyHz;
    if (BUZZER_IS_PASSIVE) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWriteTone(PIN_BUZZER, state ? frequencyHz : 0);
#else
        ledcWriteTone(BUZZER_CHANNEL, state ? frequencyHz : 0);
#endif
    } else {
        digitalWrite(PIN_BUZZER, state ? HIGH : LOW);
    }
}

// Measure HC-SR04 distance
float readDistanceCm() {
    digitalWrite(PIN_US_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    unsigned long durationUs = pulseIn(PIN_US_ECHO, HIGH, 26000);
    if (durationUs == 0) return NAN; // Unknown range, not a measured clear path.
    return (float)durationUs * 0.0343f / 2.0f;
}

void setup() {
    // Establish the module's OFF level before boot delays.
    digitalWrite(PIN_VIBRATION, MOTOR_ACTIVE_LOW ? HIGH : LOW);
    pinMode(PIN_VIBRATION, OUTPUT);
    digitalWrite(PIN_VIBRATION, MOTOR_ACTIVE_LOW ? HIGH : LOW);
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

    // Dedicated channels keep passive-buzzer pitch changes away from motor PWM.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(PIN_VIBRATION, 200, 8, MOTOR_CHANNEL);
    if (BUZZER_IS_PASSIVE) ledcAttachChannel(PIN_BUZZER, 2000, 8, BUZZER_CHANNEL);
#else
    ledcSetup(MOTOR_CHANNEL, 200, 8);
    ledcAttachPin(PIN_VIBRATION, MOTOR_CHANNEL);
    if (BUZZER_IS_PASSIVE) {
        ledcSetup(BUZZER_CHANNEL, 2000, 8);
        ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
    }
#endif
    setMotorPwm(0);
    setBuzzer(false);
    // No startup actuator burst: vibration is exclusively distance-controlled.

    // Initialize I2C and MPU6050
    Serial.println("[System] Initializing MPU6050 (SDA=GPIO 4, SCL=GPIO 5)...");
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(50); // Prevent bus lockup

    if (mpu.begin(0x68, &Wire)) {
        mpuAvailable = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        Serial.println("[System] MPU6050 connected successfully at Address 0x68!");
    } else if (mpu.begin(0x69, &Wire)) {
        mpuAvailable = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        Serial.println("[System] MPU6050 connected successfully at Address 0x69!");
    } else {
        Serial.println("[System] WARNING: MPU6050 not detected at 0x68 or 0x69. Continuing in obstacle-only mode...");
    }

    // Initialize BLE Server
    initBLE();

    Serial.println("\n[System] Initialization complete. Running live obstacle loop...\n");
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

    // Refresh time after blocking sensor reads.
    now = millis();
    const ProximityFeedback feedback = feedbackForDistance(distanceCm);
    currentMotorPwm = feedback.motorDuty;
    setMotorPwm(currentMotorPwm);

    // Fall detection retains its audible warning, but cannot override motor distance gating.
    uint8_t soundMode = fallAlert ? 2 : (feedback.motorDuty > 0 ? 1 : 0);
    buzzerState = beepEnvelope.update(now,
        fallAlert ? 200 : feedback.beepOnMs,
        fallAlert ? 200 : feedback.beepOffMs, soundMode);
    setBuzzer(buzzerState, fallAlert ? 2400 : feedback.toneHz);
    digitalWrite(PIN_LED_C3, (currentMotorPwm > 0 || buzzerState) ? LOW : HIGH);

    // ------------------------------------------------------------------------
    // 4. Telemetry Stream every 250ms
    // ------------------------------------------------------------------------
    if (now - lastTelemMs >= 250) {
        lastTelemMs = now;

        const char* alertMsg = "CLEAR";
        if (fallAlert) alertMsg = "FALL ALARM!";
        else if (!isfinite(distanceCm)) alertMsg = "NO ECHO";
        else if (distanceCm <= 10.0f) alertMsg = "CRITICAL HAZARD!";
        else if (distanceCm <= 30.0f) alertMsg = "WARNING";
        else if (distanceCm < 60.0f) alertMsg = "CAUTION";

        uint8_t vibPercent = (uint8_t)((currentMotorPwm / 255.0f) * 100.0f);

        Serial.printf("[CANE-C3] Dist: %5.1f cm | Tilt: %4.1f° | Vib: %3d%% (PWM: %3d) | Buzzer: %s | BLE: %s | Alert: %s\n",
                      distanceCm,
                      tiltAngleDeg,
                      vibPercent,
                      currentMotorPwm,
                      buzzerState ? "ON" : "OFF",
                      bleClientConnected ? "CONNECTED" : "ADVERTISING",
                      alertMsg);

        // Send live telemetry to connected BLE client
        if (bleClientConnected && pTelemetryCharacteristic != nullptr) {
            char bleMsg[64];
            snprintf(bleMsg, sizeof(bleMsg), "Dist:%.1f,Tilt:%.1f,Alert:%s",
                     isfinite(distanceCm) ? distanceCm : -1.0f,
                     tiltAngleDeg,
                     alertMsg);
            pTelemetryCharacteristic->setValue(bleMsg);
            pTelemetryCharacteristic->notify();
        }
    }
}
