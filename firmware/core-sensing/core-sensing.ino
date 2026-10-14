/**
 * ============================================================================
 * IntelliCane — Multi-Modal Sensing & Reflex Firmware (ESP32-C3 SuperMini)
 * Neural-Nexus | IoTrix 2.0 (Track A Embedded IoT System Development)
 * 
 * Hardware Architecture (1× TOF10120 + 1× Ultrasonic + 1× MPU6050):
 *   - TOF10120 Laser Time-of-Flight Sensor (I2C Addr 0x52):
 *       High-speed millimeter optical ranging.
 *       SDA -> GPIO 4 (Blue wire), SCL -> GPIO 5 (Green wire)
 *   - Ultrasonic Distance Sensor (1× HC-SR04):
 *       Wide-angle acoustic echo ranging (detects glass, broad barriers, meshes).
 *       TRIG -> GPIO 0, ECHO -> GPIO 1
 *   - MPU-6050 (GY-521) 6-Axis Motion & Fall IMU (I2C Addr 0x68):
 *       Hardware I2C shared on SDA -> GPIO 4, SCL -> GPIO 5
 *   - Dual Haptic Vibration Motors:
 *       Motor 1 (Primary Proximity): GPIO 6 (LEDC Channel 0, 20 kHz silent PWM)
 *       Motor 2 (Dual Proximity):    GPIO 2 (LEDC Channel 1, 20 kHz silent PWM)
 *   - Gentle Alert Buzzer:           GPIO 7 (Constant 800 Hz gentle chime, proximity-accelerated pulses)
 *   - Pushbutton (Tactile Switch):   Freed / Not connected (AI triggered via Serial/BLE)
 *   - ESP32-CAM AI Vision Trigger:   GPIO 21 (Pulses LOW to ESP32-CAM GPIO 13)
 *   - Onboard Status LED:            Freed / Not connected
 *   - Bluetooth Low Energy:          Nordic UART Service (NUS) Telemetry & Control
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
constexpr uint8_t PIN_US_TRIG     = 0;   // Ultrasonic Trigger (HC-SR04)
constexpr uint8_t PIN_US_ECHO     = 1;   // Ultrasonic Echo (HC-SR04)
constexpr uint8_t PIN_I2C_SDA     = 4;   // Shared I2C SDA (TOF10120 0x52 + MPU6050 0x68)
constexpr uint8_t PIN_I2C_SCL     = 5;   // Shared I2C SCL (TOF10120 0x52 + MPU6050 0x68)
constexpr uint8_t PIN_MOTOR_FWD   = 6;   // Motor 1: Primary Proximity Haptic (LEDC Ch 0)
constexpr uint8_t PIN_BUZZER      = 7;   // Buzzer (Constant 800 Hz comfortable tone)
constexpr uint8_t PIN_MOTOR_GND   = 2;   // Motor 2: Dual Proximity Haptic (LEDC Ch 1)
constexpr uint8_t PIN_CAM_TRIG    = 21;  // Hardware snapshot pulse to ESP32-CAM GPIO 13

// Motor & Buzzer Configuration
constexpr bool MOTOR_ACTIVE_LOW   = false;
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
uint8_t tofFailCount = 0;
float tofDistanceCm = NAN;

float distUsCm = NAN; // Forward Ultrasonic reading

float tiltAngleDeg = 0.0f;
float gMagnitude = 1.0f;
bool fallAlert = false;

uint8_t currentMotorFwdPwm = 0;
uint8_t currentMotorGndPwm = 0;
bool buzzerState = false;

uint32_t lastSensorMs = 0;
uint32_t lastTelemMs  = 0;

// Forward declarations
void blePrint(const char* msg);
void blePrintln(const char* msg);
void triggerCameraSnapshot();
void setMotorFwdPwm(uint8_t duty);
void setMotorGndPwm(uint8_t duty);
void setBuzzer(bool state, uint16_t frequencyHz = PLEASANT_ALERT_TONE_HZ);
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
        blePrintln("  INTELLICANE — MULTI-MODAL FUSED TELEMETRY & CONTROL");
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
        (!state || frequencyHz == previousFrequency)) return;
    initialized = true;
    previousState = state;
    previousFrequency = frequencyHz;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(PIN_BUZZER, state ? frequencyHz : 0);
#else
    ledcWriteTone(BUZZER_CHANNEL, state ? frequencyHz : 0);
#endif
}

// ============================================================================
// Ultrasonic Ranging Helper (1× HC-SR04)
// ============================================================================
float readUltrasonicCm(uint8_t trigPin, uint8_t echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // 15ms timeout allows measuring up to ~2.5m without blocking the CPU loop
    unsigned long durationUs = pulseIn(echoPin, HIGH, 15000);
    // Ignore floating noise glitches (<180us is <3cm, inside transducer blind zone) or timeout (0)
    if (durationUs < 180 || durationUs >= 15000) return NAN;
    float dist = (float)durationUs * 0.0343f / 2.0f;
    if (dist < 3.0f || dist > 300.0f) return NAN;
    return dist;
}

// ============================================================================
// TOF10120 Time-of-Flight Laser Sensor (I2C Addr 0x52)
// High-Speed Millimeter-Accurate Optical Ranging
// ============================================================================
bool readTOF10120(float &distCm) {
    if (!tofAvailable) return false;

    Wire.beginTransmission(0x52);
    Wire.write(0x00);
    uint8_t status = Wire.endTransmission();
    if (status != 0) {
        tofFailCount++;
        if (tofFailCount >= 5) {
            tofAvailable = false;
            tofFailCount = 0;
        }
        return false;
    }

    // Micro-delay to allow sensor register update
    delay(1);

    if (Wire.requestFrom((uint8_t)0x52, (uint8_t)2) != 2) {
        tofFailCount++;
        if (tofFailCount >= 5) {
            tofAvailable = false;
            tofFailCount = 0;
        }
        return false;
    }

    tofFailCount = 0;
    uint16_t distMm = (Wire.read() << 8) | Wire.read();

    // TOF10120 returns 0, >=1800, or 2000 when out of range
    if (distMm == 0 || distMm >= 1800) {
        distCm = NAN; // Clear / no obstacle within ~1.8m
    } else {
        distCm = (float)distMm / 10.0f; // Convert mm to cm
    }
    return true;
}

bool tryInitTOF(bool verbose) {
    Wire.beginTransmission(0x52);
    if (Wire.endTransmission() == 0) {
        tofAvailable = true;
        tofFailCount = 0;
        if (verbose) Serial.println("[TOF] TOF10120 Laser Distance Sensor detected at I2C 0x52!");
        return true;
    }
    tofAvailable = false;
    return false;
}

// ============================================================================
// MPU6050 Motion & Fall IMU (I2C Addr 0x68 / 0x69)
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
            Serial.printf("[I2C] -> Found device at address 0x%02X", addr);
            if (addr == 0x52) Serial.print(" (TOF10120 Laser Distance Sensor)");
            else if (addr == 0x68 || addr == 0x69) Serial.print(" (MPU6050 Motion IMU)");
            Serial.println();
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

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // Single Ultrasonic Pins Setup (HC-SR04 with internal pull-down against floating noise)
    pinMode(PIN_US_TRIG, OUTPUT);
    pinMode(PIN_US_ECHO, INPUT_PULLDOWN);
    digitalWrite(PIN_US_TRIG, LOW);

    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("  INTELLICANE — FUSED SENSOR SYSTEM (1× TOF + 1× US)  ");
    Serial.println("=======================================================");

    // Configure LEDC PWM Channels
    // 20 kHz PWM for vibration motors: Ultrasonic & silent to human ears
    // (eliminates 200 Hz audible motor coil whine/beeping)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(PIN_MOTOR_FWD, 20000, 8, MOTOR_FWD_CHANNEL);
    ledcAttachChannel(PIN_MOTOR_GND, 20000, 8, MOTOR_GND_CHANNEL);
    ledcAttachChannel(PIN_BUZZER, PLEASANT_ALERT_TONE_HZ, 8, BUZZER_CHANNEL);
#else
    ledcSetup(MOTOR_FWD_CHANNEL, 20000, 8);
    ledcAttachPin(PIN_MOTOR_FWD, MOTOR_FWD_CHANNEL);
    ledcSetup(MOTOR_GND_CHANNEL, 20000, 8);
    ledcAttachPin(PIN_MOTOR_GND, MOTOR_GND_CHANNEL);
    ledcSetup(BUZZER_CHANNEL, PLEASANT_ALERT_TONE_HZ, 8);
    ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
#endif
    setMotorFwdPwm(0);
    setMotorGndPwm(0);
    setBuzzer(false);

    // Startup confirmation: gentle 40ms 800 Hz pip & dual haptic pulse
    setBuzzer(true, PLEASANT_ALERT_TONE_HZ);
    setMotorFwdPwm(120);
    setMotorGndPwm(120);
    delay(40);
    setBuzzer(false);
    setMotorFwdPwm(0);
    setMotorGndPwm(0);

    // Initialize I2C Bus (SDA=GPIO 4, SCL=GPIO 5)
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

        // Read single forward HC-SR04 ultrasonic sensor
        distUsCm = readUltrasonicCm(PIN_US_TRIG, PIN_US_ECHO);

        // Read TOF10120 Laser Distance Sensor (I2C 0x52)
        if (tofAvailable) {
            float tDist = NAN;
            if (readTOF10120(tDist)) {
                tofDistanceCm = tDist;
            }
        } else {
            tofDistanceCm = NAN;
        }

        // Serial Command Monitor (Bench testing without button: type 'c' or 'describe' to trigger camera)
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            cmd.toLowerCase();
            if (cmd == "c" || cmd == "cam" || cmd == "capture" || cmd == "describe") {
                Serial.println("[Serial] AI Camera capture commanded via Serial Monitor!");
                triggerCameraSnapshot();
                if (bleClientConnected) {
                    blePrintln(">>> [AI VISION] Camera capture triggered via Serial <<<");
                }
            } else if (cmd == "r" || cmd == "reset") {
                fallAlert = false;
                Serial.println("[Serial] Fall alert cleared via Serial Monitor.");
                if (bleClientConnected) {
                    blePrintln("FALL_STATE:0");
                }
            }
        }

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

        // Fall State Transition Dispatch over BLE (strictly backward-compatible)
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
    // 2. Multi-Modal Sensor Fusion (TOF Laser + Ultrasonic Sound Waves)
    // ------------------------------------------------------------------------
    // Fuses optical TOF laser ranging with acoustic ultrasonic ranging.
    // - Laser detects narrow/sharp obstacles with high precision.
    // - Ultrasound detects wide barriers, walls, and glass/transparent obstacles.
    // The closest detected obstacle takes priority for user safety.
    now = millis();

    float minObstacleDistCm = NAN;

    if (isfinite(tofDistanceCm) && tofDistanceCm > 0.0f) {
        minObstacleDistCm = tofDistanceCm;
    }

    if (isfinite(distUsCm) && distUsCm > 0.0f) {
        if (!isfinite(minObstacleDistCm) || distUsCm < minObstacleDistCm) {
            minObstacleDistCm = distUsCm;
        }
    }

    const ProximityFeedback feedback = feedbackForDistance(minObstacleDistCm);

    if (fallAlert) {
        // Silent fall mode on cane (no buzzer or motor to prevent user distress)
        currentMotorFwdPwm = 0;
        currentMotorGndPwm = 0;
        setMotorFwdPwm(0);
        setMotorGndPwm(0);
        buzzerState = false;
        setBuzzer(false);
    } else {
        // --- Dual Vibration Motors: Proportional Haptic Feedback (0 to 60 cm) ---
        // Both motors operate synchronously to provide robust tactile feedback to the user's hand
        currentMotorFwdPwm = feedback.motorDuty;
        currentMotorGndPwm = feedback.motorDuty;
        setMotorFwdPwm(currentMotorFwdPwm);
        setMotorGndPwm(currentMotorGndPwm);

        // --- Buzzer Audio Feedback ---
        buzzerState = beepEnvelope.update(now, feedback.beepOnMs, feedback.beepOffMs, feedback.motorDuty > 0 ? 1 : 0);
        setBuzzer(buzzerState, feedback.toneHz);
    }

    // ------------------------------------------------------------------------
    // 3. Telemetry Stream every 250 ms (USB Serial & BLE NUS)
    // ------------------------------------------------------------------------
    if (now - lastTelemMs >= 250) {
        lastTelemMs = now;

        const char* alertMsg = "CLEAR";
        if (fallAlert) alertMsg = "FALL ALARM!";
        else if (!isfinite(minObstacleDistCm)) alertMsg = "CLEAR (>1.8m)";
        else if (minObstacleDistCm <= 10.0f) alertMsg = "CRITICAL HAZARD!";
        else if (minObstacleDistCm <= 25.0f) alertMsg = "WARNING (CLOSE)";
        else if (minObstacleDistCm < 60.0f) alertMsg = "CAUTION";

        char distBuf[16];
        if (isfinite(minObstacleDistCm)) snprintf(distBuf, sizeof(distBuf), "%5.1fcm", minObstacleDistCm);
        else snprintf(distBuf, sizeof(distBuf), "CLEAR");

        char tofBuf[16];
        if (tofAvailable && isfinite(tofDistanceCm)) snprintf(tofBuf, sizeof(tofBuf), "%5.1fcm", tofDistanceCm);
        else if (tofAvailable) snprintf(tofBuf, sizeof(tofBuf), "CLEAR");
        else snprintf(tofBuf, sizeof(tofBuf), "NO_TOF");

        char usBuf[16];
        if (isfinite(distUsCm)) snprintf(usBuf, sizeof(usBuf), "%4.1fcm", distUsCm);
        else snprintf(usBuf, sizeof(usBuf), "NO_ECHO");

        char tiltBuf[16];
        if (mpuAvailable) snprintf(tiltBuf, sizeof(tiltBuf), "%5.1f°", tiltAngleDeg);
        else snprintf(tiltBuf, sizeof(tiltBuf), "NO_MPU");

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

        uint8_t vibPercent = (uint8_t)((currentMotorFwdPwm / 255.0f) * 100.0f);

        char telemLine[230];
        snprintf(telemLine, sizeof(telemLine),
                 "[CANE-C3] Dist:%s | ToF:%s | US:%s | Tilt:%s | Fall:%d | Vib:%3d%% (M1:%3d/M2:%3d) | Buzz:%s | %s",
                 distBuf,
                 tofBuf,
                 usBuf,
                 tiltBuf,
                 fallAlert ? 1 : 0,
                 vibPercent,
                 currentMotorFwdPwm,
                 currentMotorGndPwm,
                 buzzerState ? "ON" : "OFF",
                 alertMsg);

        Serial.println(telemLine);

        if (bleClientConnected) {
            blePrintln(telemLine);
        }
    }
}
