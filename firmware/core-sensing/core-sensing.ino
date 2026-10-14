/**
 * ============================================================================
 * IntelliCane — Advanced Multi-Sensor Core Firmware (ESP32-C3 SuperMini)
 * Neural-Nexus | IoTrix 2.0 (Track A Embedded IoT System Development)
 * 
 * Full Hardware Integration:
 *   - Dual Ultrasonic (HC-SR04):
 *       US1 (Forward/Chest):  TRIG -> GPIO 0, ECHO -> GPIO 1
 *       US2 (Lower/Ground):   TRIG -> GPIO 2, ECHO -> GPIO 10
 *   - TOF10120 Laser ToF:    SDA -> GPIO 4, SCL -> GPIO 5 (I2C Addr 0x52)
 *   - MPU-6050 (GY-521) IMU:  SDA -> GPIO 4, SCL -> GPIO 5 (I2C Addr 0x68)
 *   - Dual Haptic Motors:
 *       Motor 1 (Forward Obstacle):   GPIO 6 (LEDC Channel 0, 200 Hz)
 *       Motor 2 (Ground / Drop-off):  GPIO 9 (LEDC Channel 1, 200 Hz)
 *   - Piezo Buzzer:                   GPIO 7 (Hazard audio & fall alarm)
 *   - Pushbutton (Tactile Switch):    GPIO 3 (Input Pullup; alarm reset & AI trigger)
 *   - ESP32-CAM AI Vision Trigger:    GPIO 21 (Pulses LOW to ESP32-CAM GPIO 13)
 *   - Onboard Status LED:             GPIO 8 (Active LOW)
 *   - Bluetooth Low Energy:           Nordic UART Service (NUS) Telemetry & Control
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

// ============================================================================
// Pin Definitions (ESP32-C3 SuperMini)
// ============================================================================
constexpr uint8_t PIN_US1_TRIG    = 0;   // Ultrasonic 1 (Forward/Chest) Trigger
constexpr uint8_t PIN_US1_ECHO    = 1;   // Ultrasonic 1 (Forward/Chest) Echo
constexpr uint8_t PIN_US2_TRIG    = 2;   // Ultrasonic 2 (Lower/Ground) Trigger
constexpr uint8_t PIN_US2_ECHO    = 10;  // Ultrasonic 2 (Lower/Ground) Echo
constexpr uint8_t PIN_BUTTON      = 3;   // Pushbutton (Input Pullup)
constexpr uint8_t PIN_I2C_SDA     = 4;   // Shared I2C SDA (MPU6050 + TOF10120)
constexpr uint8_t PIN_I2C_SCL     = 5;   // Shared I2C SCL (MPU6050 + TOF10120)
constexpr uint8_t PIN_MOTOR_FWD   = 6;   // Motor 1: Forward Obstacle Proximity (LEDC Ch 0)
constexpr uint8_t PIN_BUZZER      = 7;   // Piezo Buzzer
constexpr uint8_t PIN_LED_C3      = 8;   // Onboard LED (Active LOW)
constexpr uint8_t PIN_MOTOR_GND   = 9;   // Motor 2: Ground Hazard / Drop-off (LEDC Ch 1)
constexpr uint8_t PIN_CAM_TRIG    = 21;  // Hardware pulse to ESP32-CAM GPIO 13

// Motor & Buzzer Configuration
constexpr bool MOTOR_ACTIVE_LOW  = false;
constexpr bool BUZZER_IS_PASSIVE = false;
constexpr uint8_t MOTOR_FWD_CHANNEL = 0;
constexpr uint8_t MOTOR_GND_CHANNEL = 1;
constexpr uint8_t BUZZER_CHANNEL    = 2;

// Nordic UART Service (NUS) UUIDs
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 -> Phone
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Phone -> ESP32

bool bleClientConnected = false;
String connectedClientMac = "";
BLEServer *pBleServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pRxCharacteristic = nullptr;

BeepEnvelope beepEnvelope;

// Sensor States
Adafruit_MPU6050 mpu;
bool mpuAvailable = false;
uint8_t mpuAddress = 0x68;
bool mpuUsingDirectI2C = false;
uint8_t mpuFailCount = 0;
uint32_t lastMpuRetryMs = 0;

bool tofAvailable = false;
uint32_t lastTofRetryMs = 0;
float tofDistanceCm = NAN;
const float TOF_GROUND_BASELINE_CM = 115.0f; // Typical distance to ground when cane is held at 45 deg

float distUs1Cm = NAN; // Forward / Chest
float distUs2Cm = NAN; // Lower / Ground
float tiltAngleDeg = 0.0f;
float gMagnitude = 1.0f;
bool fallAlert = false;

uint8_t currentMotorFwdPwm = 0;
uint8_t currentMotorGndPwm = 0;
bool buzzerState = false;

uint32_t lastSensorMs = 0;
uint32_t lastTelemMs  = 0;
uint8_t usCycleToggle = 0; // Alternates pinging US1 and US2 to prevent cross-talk

// Forward declarations
void blePrint(const char* msg);
void blePrintln(const char* msg);
void triggerCameraSnapshot();
void setMotorFwdPwm(uint8_t duty);
void setMotorGndPwm(uint8_t duty);
void setBuzzer(bool state, uint16_t frequencyHz = 2000);
float readUltrasonicCm(uint8_t trigPin, uint8_t echoPin);
bool readTOF10120(float &distCm);
bool tryInitMPU(bool verbose = true);
bool tryInitTOF(bool verbose = true);
void scanI2C();

// ============================================================================
// BLE Callbacks & Transmitter
// ============================================================================
void blePrint(const char* msg) {
    if (!bleClientConnected || pTxCharacteristic == nullptr) return;
    size_t len = strlen(msg);
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > 20) chunk = 20;
        pTxCharacteristic->setValue((uint8_t*)(msg + offset), chunk);
        pTxCharacteristic->notify();
        offset += chunk;
        if (offset < len) delay(4);
    }
}

void blePrintln(const char* msg) {
    char buf[200];
    snprintf(buf, sizeof(buf), "%s\r\n", msg);
    blePrint(buf);
}

class CaneRxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            String lower = rxValue;
            lower.toLowerCase();
            Serial.printf("[BLE RX] Command received: %s\n", rxValue.c_str());

            if (lower.indexOf("reset") >= 0 || lower == "r") {
                fallAlert = false;
                Serial.println("[BLE] Fall alert cleared via wireless command.");
                blePrintln("[CANE-C3] Fall alert cleared via wireless command.");
                blePrintln("FALL_STATE:0");
            } else if (lower.indexOf("describe") >= 0 || lower.indexOf("cam") >= 0 || lower.indexOf("capture") >= 0) {
                Serial.println("[BLE] AI camera capture triggered via Bluetooth command!");
                triggerCameraSnapshot();
                blePrintln(">>> [AI VISION] Camera capture triggered! Analyzing surroundings... <<<");
            }
        }
    }
};

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
        Serial.printf("\n[BLE] >>> CLIENT CONNECTED! (%s) <<<\n", connectedClientMac.c_str());
        blePrintln("=======================================================");
        blePrintln("  INTELLICANE — MULTI-SENSOR WIRELESS TELEMETRY");
        blePrintln("=======================================================");
    }

    void onDisconnect(BLEServer* pServer, ble_gap_conn_desc *desc) override {
        bleClientConnected = false;
        connectedClientMac = "";
        Serial.println("[BLE] <<< CLIENT DISCONNECTED! Restarting advertising... >>>");
        BLEDevice::startAdvertising();
    }
#endif

    void onDisconnect(BLEServer* pServer) override {
        bleClientConnected = false;
#if !defined(CONFIG_NIMBLE_ENABLED)
        connectedClientMac = "";
        Serial.println("[BLE] <<< CLIENT DISCONNECTED! Restarting advertising... >>>");
        BLEDevice::startAdvertising();
#endif
    }
};

void initBLE() {
    Serial.println("[BLE] Initializing Bluetooth Low Energy (BLE)...");
    BLEDevice::init("Intelligent-Cane");
    BLEDevice::setMTU(256);

    pBleServer = BLEDevice::createServer();
    pBleServer->setCallbacks(new CaneBLECallbacks());

    BLEService *pService = pBleServer->createService(NUS_SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        NUS_TX_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    pRxCharacteristic = pService->createCharacteristic(
        NUS_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new CaneRxCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Wireless Serial active. Discoverable as 'Intelligent-Cane'.");
}

// ============================================================================
// Hardware Camera Interconnect (Triggering ESP32-CAM)
// ============================================================================
void triggerCameraSnapshot() {
    Serial.println("[INTER-MCU] Sending capture trigger pulse to ESP32-CAM (GPIO 21 -> GPIO 13)...");
    digitalWrite(PIN_CAM_TRIG, LOW); // Active LOW trigger
    delay(80);
    digitalWrite(PIN_CAM_TRIG, HIGH);
}

// ============================================================================
// Motor & Buzzer Outputs
// ============================================================================
void setMotorFwdPwm(uint8_t duty) {
    uint8_t output = motorOutputDuty(duty, MOTOR_ACTIVE_LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_MOTOR_FWD, output);
#else
    ledcWrite(MOTOR_FWD_CHANNEL, output);
#endif
}

void setMotorGndPwm(uint8_t duty) {
    uint8_t output = motorOutputDuty(duty, MOTOR_ACTIVE_LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_MOTOR_GND, output);
#else
    ledcWrite(MOTOR_GND_CHANNEL, output);
#endif
}

void setBuzzer(bool state, uint16_t frequencyHz) {
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

// ============================================================================
// Ultrasonic Ranging Helper
// ============================================================================
float readUltrasonicCm(uint8_t trigPin, uint8_t echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    unsigned long durationUs = pulseIn(echoPin, HIGH, 26000);
    if (durationUs == 0) return NAN;
    return (float)durationUs * 0.0343f / 2.0f;
}

// ============================================================================
// TOF10120 Time-of-Flight Laser Sensor (I2C Addr 0x52)
// ============================================================================
bool readTOF10120(float &distCm) {
    if (!tofAvailable) return false;

    Wire.beginTransmission(0x52);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom((uint8_t)0x52, (uint8_t)2) != 2) return false;

    uint16_t distMm = (Wire.read() << 8) | Wire.read();
    if (distMm == 0 || distMm > 2000) {
        distCm = 200.0f; // Saturated or out-of-range (indicates floor drop-off)
    } else {
        distCm = (float)distMm / 10.0f;
    }
    return true;
}

bool tryInitTOF(bool verbose) {
    Wire.beginTransmission(0x52);
    if (Wire.endTransmission() == 0) {
        tofAvailable = true;
        if (verbose) Serial.println("[TOF] TOF10120 Laser Distance Sensor detected at I2C 0x52!");
        return true;
    }
    tofAvailable = false;
    return false;
}

// ============================================================================
// MPU6050 Motion & Fall IMU
// ============================================================================
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

bool tryInitMPU(bool verbose) {
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
    if (mpu.begin(targetAddr, &Wire)) {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpuAvailable = true;
        mpuUsingDirectI2C = false;
        if (verbose) Serial.printf("[MPU] Initialized at 0x%02X via Adafruit driver.\n", targetAddr);
        return true;
    }

    // Direct register fallback
    writeReg8(targetAddr, 0x6B, 0x00);
    delay(10);
    writeReg8(targetAddr, 0x1C, 0x10);
    delay(10);
    mpuAvailable = true;
    mpuUsingDirectI2C = true;
    if (verbose) Serial.printf("[MPU] Initialized at 0x%02X via direct registers.\n", targetAddr);
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
    Wire.write(0x3B);
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

void scanI2C() {
    Serial.println("[I2C] Scanning I2C bus (SDA=GPIO 4, SCL=GPIO 5)...");
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] -> Found device at address 0x%02X\n", addr);
            count++;
        }
    }
    if (count == 0) {
        Serial.println("[I2C] WARNING: No I2C devices found on bus.");
    }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    // Motor initial off levels
    pinMode(PIN_MOTOR_FWD, OUTPUT);
    pinMode(PIN_MOTOR_GND, OUTPUT);
    digitalWrite(PIN_MOTOR_FWD, MOTOR_ACTIVE_LOW ? HIGH : LOW);
    digitalWrite(PIN_MOTOR_GND, MOTOR_ACTIVE_LOW ? HIGH : LOW);

    // Camera Trigger line (Default HIGH)
    pinMode(PIN_CAM_TRIG, OUTPUT);
    digitalWrite(PIN_CAM_TRIG, HIGH);

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    pinMode(PIN_LED_C3, OUTPUT);
    digitalWrite(PIN_LED_C3, HIGH); // Off

    // Ultrasonic Pins
    pinMode(PIN_US1_TRIG, OUTPUT);
    pinMode(PIN_US1_ECHO, INPUT);
    pinMode(PIN_US2_TRIG, OUTPUT);
    pinMode(PIN_US2_ECHO, INPUT);
    digitalWrite(PIN_US1_TRIG, LOW);
    digitalWrite(PIN_US2_TRIG, LOW);

    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("  INTELLICANE — ADVANCED MULTI-SENSOR SYSTEM (C3)     ");
    Serial.println("=======================================================");

    // Configure LEDC PWM Channels
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(PIN_MOTOR_FWD, 200, 8, MOTOR_FWD_CHANNEL);
    ledcAttachChannel(PIN_MOTOR_GND, 200, 8, MOTOR_GND_CHANNEL);
    if (BUZZER_IS_PASSIVE) ledcAttachChannel(PIN_BUZZER, 2000, 8, BUZZER_CHANNEL);
#else
    ledcSetup(MOTOR_FWD_CHANNEL, 200, 8);
    ledcAttachPin(PIN_MOTOR_FWD, MOTOR_FWD_CHANNEL);
    ledcSetup(MOTOR_GND_CHANNEL, 200, 8);
    ledcAttachPin(PIN_MOTOR_GND, MOTOR_GND_CHANNEL);
    if (BUZZER_IS_PASSIVE) {
        ledcSetup(BUZZER_CHANNEL, 2000, 8);
        ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
    }
#endif
    setMotorFwdPwm(0);
    setMotorGndPwm(0);
    setBuzzer(false);

    // Initialize I2C Bus
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(40);

    scanI2C();
    tryInitMPU(true);
    tryInitTOF(true);

    initBLE();

    Serial.println("\n[System] All modules configured. Running intelligent cane loop...\n");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    uint32_t now = millis();

    // ------------------------------------------------------------------------
    // 1. Read Sensors every 35 ms
    // ------------------------------------------------------------------------
    if (now - lastSensorMs >= 35) {
        lastSensorMs = now;

        // Alternate Ultrasonic pings to completely eliminate sonic cross-talk
        if (usCycleToggle == 0) {
            distUs1Cm = readUltrasonicCm(PIN_US1_TRIG, PIN_US1_ECHO);
            usCycleToggle = 1;
        } else {
            distUs2Cm = readUltrasonicCm(PIN_US2_TRIG, PIN_US2_ECHO);
            usCycleToggle = 0;
        }

        // Read TOF10120 Laser Distance Sensor (I2C)
        if (tofAvailable) {
            float tDist = NAN;
            if (readTOF10120(tDist)) {
                tofDistanceCm = tDist;
            }
        }

        // Pushbutton Monitor: Short click resets alarm; Pressing button also triggers AI camera
        static uint32_t buttonPressStart = 0;
        static bool lastButtonState = HIGH;
        bool btnState = digitalRead(PIN_BUTTON);

        if (btnState == LOW && lastButtonState == HIGH) {
            buttonPressStart = now;
        } else if (btnState == HIGH && lastButtonState == LOW) {
            uint32_t pressDuration = now - buttonPressStart;
            if (fallAlert) {
                fallAlert = false;
                Serial.println("[Button] Fall alarm reset.");
                if (bleClientConnected) {
                    blePrintln(">>> [BUTTON: ALARM CLEARED] Fall reset via button <<<");
                    blePrintln("FALL_STATE:0");
                }
            } else if (pressDuration > 40) {
                Serial.println("[Button] AI Camera scene description requested via cane button!");
                triggerCameraSnapshot();
                if (bleClientConnected) {
                    blePrintln(">>> [AI VISION] Cane button clicked! Triggering camera snapshot... <<<");
                }
            }
        }
        lastButtonState = btnState;

        // MPU6050 Motion & Fall Monitoring
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

        // Fall State Transition Dispatch over BLE
        static bool previousFallState = false;
        if (fallAlert != previousFallState) {
            previousFallState = fallAlert;
            if (fallAlert) {
                Serial.printf("\n[FALL] >>> FALL ALARM! Tilt: %.1f deg | Impact: %.2f g <<<\n\n", tiltAngleDeg, gMagnitude);
                if (bleClientConnected) {
                    char fallMsg[96];
                    snprintf(fallMsg, sizeof(fallMsg), "!!! [ALERT: FALL DETECTED!] Tilt: %.1f deg | Force: %.2f g !!!", tiltAngleDeg, gMagnitude);
                    blePrintln(fallMsg);
                    blePrintln("FALL_STATE:1");
                }
            } else {
                Serial.println("\n[FALL] <<< CANE UPRIGHT / FALL CLEARED >>>\n");
                if (bleClientConnected) {
                    blePrintln(">>> [ALERT: CANE RESTORED UPRIGHT] Fall Cleared <<<");
                    blePrintln("FALL_STATE:0");
                }
            }
        }
    }

    // Auto-reconnect background retry for I2C modules
    if (!mpuAvailable && (now - lastMpuRetryMs >= 2000)) {
        lastMpuRetryMs = now;
        tryInitMPU(false);
    }
    if (!tofAvailable && (now - lastTofRetryMs >= 3000)) {
        lastTofRetryMs = now;
        tryInitTOF(false);
    }

    // ------------------------------------------------------------------------
    // 2. Dual-Zone Haptic & Audio Feedback Arbitration
    // ------------------------------------------------------------------------
    now = millis();

    // Determine closest forward obstacle from both ultrasonic sensors
    float minFwdDistance = distUs1Cm;
    if (isfinite(distUs2Cm) && distUs2Cm > 0.0f) {
        if (!isfinite(minFwdDistance) || distUs2Cm < minFwdDistance) {
            minFwdDistance = distUs2Cm;
        }
    }

    const ProximityFeedback fwdFeedback = feedbackForDistance(minFwdDistance);
    const GroundHazardFeedback gndFeedback = feedbackForGround(tofDistanceCm, TOF_GROUND_BASELINE_CM);

    if (fallAlert) {
        // Silent fall mode on cane (no buzzer or motor to prevent user distress)
        currentMotorFwdPwm = 0;
        currentMotorGndPwm = 0;
        setMotorFwdPwm(0);
        setMotorGndPwm(0);
        buzzerState = false;
        setBuzzer(false);
        digitalWrite(PIN_LED_C3, LOW); // LED active indicates emergency
    } else {
        // --- Forward Obstacle Haptics (Motor 1) ---
        currentMotorFwdPwm = fwdFeedback.motorDuty;
        setMotorFwdPwm(currentMotorFwdPwm);

        // --- Ground Hazard / Drop-off Haptics (Motor 2) ---
        if (gndFeedback.type == GroundHazardType::DROP_OFF) {
            // Rhythmic warning pulse on Motor 2 for drop-off/stairs down
            static bool gndPulseState = false;
            static uint32_t lastGndPulseMs = 0;
            uint32_t interval = gndPulseState ? gndFeedback.pulseOnMs : gndFeedback.pulseOffMs;
            if (now - lastGndPulseMs >= interval) {
                lastGndPulseMs = now;
                gndPulseState = !gndPulseState;
            }
            currentMotorGndPwm = gndPulseState ? gndFeedback.motorDuty : 0;
            setMotorGndPwm(currentMotorGndPwm);
        } else if (gndFeedback.type == GroundHazardType::STEP_UP) {
            currentMotorGndPwm = gndFeedback.motorDuty;
            setMotorGndPwm(currentMotorGndPwm);
        } else {
            currentMotorGndPwm = 0;
            setMotorGndPwm(0);
        }

        // --- Buzzer Audio Feedback ---
        // Beeps if forward obstacle is in range or if severe drop-off detected
        uint8_t buzzMode = (fwdFeedback.motorDuty > 0) ? 1 : (gndFeedback.type == GroundHazardType::DROP_OFF ? 2 : 0);
        uint16_t onTime  = (buzzMode == 2) ? 150 : fwdFeedback.beepOnMs;
        uint16_t offTime = (buzzMode == 2) ? 150 : fwdFeedback.beepOffMs;
        uint16_t pitch   = (buzzMode == 2) ? 2800 : fwdFeedback.toneHz;

        buzzerState = beepEnvelope.update(now, onTime, offTime, buzzMode);
        setBuzzer(buzzerState, pitch);

        digitalWrite(PIN_LED_C3, (currentMotorFwdPwm > 0 || currentMotorGndPwm > 0 || buzzerState) ? LOW : HIGH);
    }

    // ------------------------------------------------------------------------
    // 3. Telemetry Stream every 250 ms (USB Serial & BLE NUS)
    // ------------------------------------------------------------------------
    if (now - lastTelemMs >= 250) {
        lastTelemMs = now;

        const char* alertMsg = "CLEAR";
        if (fallAlert) alertMsg = "FALL ALARM!";
        else if (gndFeedback.type == GroundHazardType::DROP_OFF) alertMsg = "DROP-OFF HAZARD!";
        else if (gndFeedback.type == GroundHazardType::STEP_UP) alertMsg = "CURB / STEP-UP";
        else if (!isfinite(minFwdDistance)) alertMsg = "NO ECHO";
        else if (minFwdDistance <= 10.0f) alertMsg = "CRITICAL HAZARD!";
        else if (minFwdDistance <= 30.0f) alertMsg = "WARNING";
        else if (minFwdDistance < 60.0f) alertMsg = "CAUTION";

        char tiltBuf[16];
        if (mpuAvailable) snprintf(tiltBuf, sizeof(tiltBuf), "%5.1f°", tiltAngleDeg);
        else snprintf(tiltBuf, sizeof(tiltBuf), "NO_MPU");

        char tofBuf[16];
        if (tofAvailable && isfinite(tofDistanceCm)) snprintf(tofBuf, sizeof(tofBuf), "%5.1fcm", tofDistanceCm);
        else snprintf(tofBuf, sizeof(tofBuf), "NO_TOF");

        char telemLine[210];
        snprintf(telemLine, sizeof(telemLine),
                 "[CANE] US1:%4.1fcm | US2:%4.1fcm | ToF:%s | Tilt:%s | Fall:%d | M1:%3d | M2:%3d | %s",
                 distUs1Cm,
                 distUs2Cm,
                 tofBuf,
                 tiltBuf,
                 fallAlert ? 1 : 0,
                 currentMotorFwdPwm,
                 currentMotorGndPwm,
                 alertMsg);

        Serial.println(telemLine);

        if (bleClientConnected) {
            blePrintln(telemLine);
        }
    }
}
