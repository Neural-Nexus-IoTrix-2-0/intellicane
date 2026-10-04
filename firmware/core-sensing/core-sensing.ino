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
String connectedClientMac = "";
BLEServer *pBleServer = nullptr;
BLECharacteristic *pTelemetryCharacteristic = nullptr;

class CaneBLECallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        bleClientConnected = true;
    }

#if defined(CONFIG_NIMBLE_ENABLED)
    void onConnect(BLEServer* pServer, ble_gap_conn_desc *desc) override {
        bleClientConnected = true;
        if (desc != nullptr) {
            BLEAddress peerAddr(desc->peer_ota_addr);
            connectedClientMac = peerAddr.toString();
        }
        Serial.println("\n=======================================================");
        Serial.printf("  [BLE] >>> CLIENT CONNECTED! (Peer MAC: %s) <<<\n",
                      connectedClientMac.length() > 0 ? connectedClientMac.c_str() : "Unknown");
        Serial.println("=======================================================\n");
    }

    void onDisconnect(BLEServer* pServer, ble_gap_conn_desc *desc) override {
        bleClientConnected = false;
        String prevMac = connectedClientMac;
        connectedClientMac = "";
        Serial.println("\n=======================================================");
        Serial.printf("  [BLE] <<< CLIENT DISCONNECTED (%s)! Restarting advertising... >>>\n",
                      prevMac.length() > 0 ? prevMac.c_str() : "Unknown");
        Serial.println("=======================================================\n");
        BLEDevice::startAdvertising();
    }
#endif

    void onDisconnect(BLEServer* pServer) override {
        bleClientConnected = false;
#if !defined(CONFIG_NIMBLE_ENABLED)
        connectedClientMac = "";
        Serial.println("\n=======================================================");
        Serial.println("  [BLE] <<< CLIENT DISCONNECTED! Restarting advertising... >>>");
        Serial.println("=======================================================\n");
        BLEDevice::startAdvertising();
#endif
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
uint8_t mpuAddress = 0;
bool mpuUsingDirectI2C = false;
uint8_t mpuFailCount = 0;
uint32_t lastMpuRetryMs = 0;

float distanceCm = NAN;
float tiltAngleDeg = 0.0f;
float gMagnitude = 1.0f;
bool fallAlert = false;
uint8_t currentMotorPwm = 0;

uint32_t lastSensorMs = 0;
uint32_t lastTelemMs  = 0;
bool buzzerState = false;

uint8_t readReg8(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    if (Wire.requestFrom((uint8_t)addr, (uint8_t)1) != 1) return 0xFF;
    return Wire.read();
}

bool writeReg8(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

uint8_t activeSda = PIN_I2C_SDA; // default 4
uint8_t activeScl = PIN_I2C_SCL; // default 5

bool probePinPair(uint8_t sda, uint8_t scl) {
    Wire.end();
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    Wire.begin(sda, scl);
    Wire.setClock(100000);
    Wire.setTimeOut(30);

    for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            activeSda = sda;
            activeScl = scl;
            return true;
        }
    }
    return false;
}

void autoDetectI2CPins() {
    if (probePinPair(activeSda, activeScl)) return;
    if (probePinPair(5, 4)) {
        Serial.println("[I2C] Auto-detect: Found MPU on swapped pins (SDA=GPIO 5, SCL=GPIO 4)!");
        return;
    }
    if (probePinPair(8, 9)) {
        Serial.println("[I2C] Auto-detect: Found MPU on hardware default pins (SDA=GPIO 8, SCL=GPIO 9)!");
        return;
    }
    if (probePinPair(9, 8)) {
        Serial.println("[I2C] Auto-detect: Found MPU on hardware default pins (SDA=GPIO 9, SCL=GPIO 8)!");
        return;
    }
    if (probePinPair(2, 3)) {
        Serial.println("[I2C] Auto-detect: Found MPU on pins (SDA=GPIO 2, SCL=GPIO 3)!");
        return;
    }
    if (probePinPair(20, 21)) {
        Serial.println("[I2C] Auto-detect: Found MPU on pins (SDA=GPIO 20, SCL=GPIO 21)!");
        return;
    }
    probePinPair(PIN_I2C_SDA, PIN_I2C_SCL);
}

void scanI2C() {
    autoDetectI2CPins();
    Serial.printf("[I2C] Scanning I2C bus (SDA=GPIO %d, SCL=GPIO %d)...\n", activeSda, activeScl);
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            uint8_t who = readReg8(addr, 0x75);
            Serial.printf("[I2C] -> Found device at 0x%02X (WHO_AM_I = 0x%02X)\n", addr, who);
            count++;
        }
    }
    if (count == 0) {
        Serial.println("[I2C] -> WARNING: No I2C devices found on tested pins!");
        Serial.println("       1. Check GY-521 LED: Is the small red power LED on the sensor lit?");
        Serial.println("       2. Power: Connect GY-521 VCC -> 5V on ESP32-C3 (3.3V can cause undervoltage).");
        Serial.println("       3. Ground: Connect GY-521 GND -> GND.");
        Serial.println("       4. Data: Connect GY-521 SDA -> GPIO 4, SCL -> GPIO 5.");
        Serial.println("       5. Address: Connect GY-521 AD0 -> GND (sets address 0x68).");
    }
}

bool tryInitMPU(bool verbose = true) {
    autoDetectI2CPins();
    uint8_t targetAddr = 0;
    Wire.beginTransmission(0x68);
    if (Wire.endTransmission() == 0) targetAddr = 0x68;
    else {
        Wire.beginTransmission(0x69);
        if (Wire.endTransmission() == 0) targetAddr = 0x69;
    }

    if (targetAddr == 0) {
        mpuAvailable = false;
        return false;
    }

    mpuAddress = targetAddr;
    uint8_t who = readReg8(targetAddr, 0x75);
    if (verbose) {
        Serial.printf("[MPU] Found chip at 0x%02X (WHO_AM_I = 0x%02X)\n", targetAddr, who);
    }

    // Try Adafruit driver first
    if (mpu.begin(targetAddr, &Wire)) {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpuAvailable = true;
        mpuUsingDirectI2C = false;
        if (verbose) Serial.println("[MPU] Initialized via Adafruit MPU6050 driver!");
        return true;
    }

    // Direct register fallback (supports MPU-6500, MPU-9250, and clone ICs rejected by Adafruit)
    if (verbose) Serial.println("[MPU] Adafruit driver rejected ID. Initializing direct register driver...");
    writeReg8(targetAddr, 0x6B, 0x00); // Wake up chip (clear SLEEP bit)
    delay(10);
    writeReg8(targetAddr, 0x1C, 0x10); // Accel range +/- 8g
    delay(10);

    mpuAvailable = true;
    mpuUsingDirectI2C = true;
    if (verbose) Serial.println("[MPU] Direct register driver active and ready!");
    return true;
}

bool readMPUAccel(float &ax, float &ay, float &az) {
    if (!mpuAvailable) return false;

    if (!mpuUsingDirectI2C) {
        sensors_event_t a, g, temp;
        if (mpu.getEvent(&a, &g, &temp)) {
            ax = a.acceleration.x;
            ay = a.acceleration.y;
            az = a.acceleration.z;
            return true;
        }
        return false;
    }

    Wire.beginTransmission(mpuAddress);
    Wire.write(0x3B); // ACCEL_XOUT_H
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)mpuAddress, (uint8_t)6) != 6) return false;

    int16_t rx = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t ry = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t rz = (int16_t)((Wire.read() << 8) | Wire.read());

    const float scale = 9.80665f / 4096.0f;
    ax = (float)rx * scale;
    ay = (float)ry * scale;
    az = (float)rz * scale;
    return true;
}

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

    // Initialize I2C bus
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(50); // Prevent bus lockup

    // Perform diagnostic I2C bus scan and initialize MPU
    scanI2C();
    if (!tryInitMPU(true)) {
        Serial.println("[System] WARNING: MPU6050 not detected. Auto-reconnect active in background...");
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

        // MPU Read
        if (mpuAvailable) {
            float ax = 0, ay = 0, az = 0;
            if (readMPUAccel(ax, ay, az)) {
                mpuFailCount = 0;
                float rawMag = sqrtf(ax * ax + ay * ay + az * az);
                gMagnitude = rawMag / 9.80665f;

                if (rawMag > 0.5f) {
                    float cosTilt = fabsf(az) / rawMag;
                    if (cosTilt > 1.0f) cosTilt = 1.0f;
                    tiltAngleDeg = acosf(cosTilt) * 180.0f / (float)M_PI;
                }

                if (tiltAngleDeg >= 65.0f || gMagnitude >= 2.5f) {
                    fallAlert = true;
                } else if (tiltAngleDeg < 30.0f) {
                    fallAlert = false;
                }
            } else {
                mpuFailCount++;
                if (mpuFailCount >= 10) {
                    mpuAvailable = false;
                    mpuFailCount = 0;
                }
            }
        }
    }

    // Background auto-reconnect if MPU was disconnected or unready at boot
    if (!mpuAvailable && (now - lastMpuRetryMs >= 2000)) {
        lastMpuRetryMs = now;
        if (tryInitMPU(false)) {
            Serial.println("\n[MPU] >>> MPU6050 RECONNECTED SUCCESSFULLY! <<<\n");
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

        char tiltBuf[16];
        if (mpuAvailable) {
            snprintf(tiltBuf, sizeof(tiltBuf), "%5.1f°", tiltAngleDeg);
        } else {
            snprintf(tiltBuf, sizeof(tiltBuf), "NO_MPU");
        }

        char bleStatusBuf[40];
        if (bleClientConnected) {
            if (connectedClientMac.length() > 0) {
                snprintf(bleStatusBuf, sizeof(bleStatusBuf), "CONN [%s]", connectedClientMac.c_str());
            } else {
                snprintf(bleStatusBuf, sizeof(bleStatusBuf), "CONNECTED");
            }
        } else {
            snprintf(bleStatusBuf, sizeof(bleStatusBuf), "ADVERTISING");
        }

        Serial.printf("[CANE-C3] Dist: %5.1f cm | Tilt: %s | Vib: %3d%% (PWM: %3d) | Buzzer: %s | BLE: %-22s | Alert: %s\n",
                      distanceCm,
                      tiltBuf,
                      vibPercent,
                      currentMotorPwm,
                      buzzerState ? "ON" : "OFF",
                      bleStatusBuf,
                      alertMsg);

        // Send live telemetry to connected BLE client
        if (bleClientConnected && pTelemetryCharacteristic != nullptr) {
            char bleMsg[64];
            snprintf(bleMsg, sizeof(bleMsg), "Dist:%.1f,Tilt:%s,Alert:%s",
                     isfinite(distanceCm) ? distanceCm : -1.0f,
                     tiltBuf,
                     alertMsg);
            pTelemetryCharacteristic->setValue(bleMsg);
            pTelemetryCharacteristic->notify();
        }
    }
}
