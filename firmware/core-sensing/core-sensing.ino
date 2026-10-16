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
 *   - Haptic Vibration Motor:       GPIO 2 (LEDC, 200 Hz brushed DC motor PWM)
 *       GPIO 6 is unused and is not configured as an actuator output.
 *   - Alert Buzzer:                  GPIO 7 (short chirps; active DC or passive 800 Hz)
 *   - Pushbutton (Tactile Switch):   GPIO 3 (Active LOW to GND, Internal pullup) + BOOT Button GPIO 9
 *   - ESP32-CAM AI Vision Trigger:   GPIO 21 (Pulses LOW to ESP32-CAM GPIO 13 or UART TX)
 *   - Onboard Status LED:            Freed / Not connected
 *   - Bluetooth Low Energy:          Nordic UART Service (NUS) Telemetry & Control
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <esp_arduino_version.h>
#include <esp_mac.h>
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
constexpr uint8_t PIN_MOTOR       = 2;   // Single proximity motor (LEDC Ch 0); GPIO 6 unused
constexpr uint8_t PIN_BUTTON      = 3;   // Push Button: Triggers AI Camera capture (Active LOW to GND)
constexpr uint8_t PIN_I2C_SDA     = 4;   // Shared I2C SDA (TOF10120 0x52 + MPU6050 0x68)
constexpr uint8_t PIN_I2C_SCL     = 5;   // Shared I2C SCL (TOF10120 0x52 + MPU6050 0x68)
constexpr uint8_t PIN_BUZZER      = 7;   // Buzzer (520 Hz low pleasant tone, high volume)
constexpr uint8_t PIN_BOOT_BTN    = 9;   // Onboard BOOT Button fallback for bench testing (Active LOW)
constexpr uint8_t PIN_CAM_RX      = 20;  // High-Speed UART RX from ESP32-CAM TX (GPIO 1)
constexpr uint8_t PIN_CAM_TX      = 21;  // High-Speed UART TX to ESP32-CAM RX (GPIO 3) or GPIO 13 pulse
constexpr uint8_t PIN_CAM_TRIG    = 21;  // Backward-compatible trigger pulse alias
HardwareSerial CamSerial(1);

// Motor & Buzzer Configuration
constexpr bool MOTOR_ACTIVE_LOW      = false;
constexpr bool BUZZER_IS_PASSIVE     = true; // 50% PWM tone creates rich low frequency with maximum acoustic volume
constexpr uint32_t MOTOR_PWM_FREQ_HZ = 200; // 200 Hz carrier frequency optimal for DC brushed vibration motors
constexpr uint8_t MOTOR_CHANNEL      = 0;
constexpr uint8_t BUZZER_CHANNEL     = 2;

// Nordic UART Service (NUS) UUIDs
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 -> Phone
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Phone -> ESP32

bool bleClientConnected = false;
String connectedClientMac = "";
BLEServer *pBleServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pRxCharacteristic = nullptr;

PulseEnvelope beepEnvelope;
PulseEnvelope motorEnvelope;

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

uint8_t currentMotorPwm = 0;
bool buzzerState = false;

uint32_t lastSensorMs = 0;
uint32_t lastTelemMs  = 0;

// Forward declarations
void blePrint(const char* msg);
void blePrintln(const char* msg);
void blePrintln(const String& msg);
void triggerCameraSnapshot();
void captureAndStreamImageToBle();
void checkPushButton();
void setMotorPwm(uint8_t duty);
void setBuzzer(bool state, uint16_t frequencyHz = PLEASANT_ALERT_TONE_HZ);
float readUltrasonicCm(uint8_t trigPin, uint8_t echoPin);
bool readTOF10120(float &distCm);
bool tryInitMPU(bool verbose = true);
bool tryInitTOF(bool verbose = true);
void scanI2C();

// ============================================================================
// Base64 Chunk Encoder for BLE Transmission
// ============================================================================
static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64EncodeChunk(const uint8_t* data, size_t length) {
    String encoded = "";
    encoded.reserve(((length + 2) / 3) * 4);
    for (size_t i = 0; i < length; i += 3) {
        uint32_t octet_a = i < length ? data[i] : 0;
        uint32_t octet_b = (i + 1) < length ? data[i + 1] : 0;
        uint32_t octet_c = (i + 2) < length ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded += B64_CHARS[(triple >> 18) & 0x3F];
        encoded += B64_CHARS[(triple >> 12) & 0x3F];
        encoded += ((i + 1) < length) ? B64_CHARS[(triple >> 6) & 0x3F] : '=';
        encoded += ((i + 2) < length) ? B64_CHARS[triple & 0x3F] : '=';
    }
    return encoded;
}

// ============================================================================
// BLE Callbacks & Transmitter
// ============================================================================
void blePrint(const char* msg) {
    if (!bleClientConnected || pTxCharacteristic == nullptr) return;
    size_t len = strlen(msg);
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > 240) chunk = 240; // Safe size within negotiated 512 MTU
        pTxCharacteristic->setValue((uint8_t*)(msg + offset), chunk);
        pTxCharacteristic->notify();
        offset += chunk;
        if (offset < len) delay(4);
    }
}

void blePrintln(const String& msg) {
    String withNewline = msg + "\r\n";
    blePrint(withNewline.c_str());
}

void blePrintln(const char* msg) {
    blePrintln(String(msg));
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
                captureAndStreamImageToBle();
            } else if (lower.indexOf("motor") >= 0 || lower == "m" || lower == "test") {
                Serial.println("[BLE] Motor diagnostic sequence requested!");
                blePrintln("[TEST] Testing motor (GPIO 2) for 500ms...");
                setMotorPwm(255);
                delay(500);
                setMotorPwm(0);
                blePrintln("[TEST] Motor diagnostic complete.");
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

    uint8_t btMac[6];
    esp_read_mac(btMac, ESP_MAC_BT);
    Serial.printf("[BLE] >>> ACTIVE BLE MAC ADDRESS: %02X:%02X:%02X:%02X:%02X:%02X <<<\n",
                  btMac[0], btMac[1], btMac[2], btMac[3], btMac[4], btMac[5]);

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
// Physical Push Button Monitor (AI Camera Snapshot Trigger)
// ============================================================================
void checkPushButton() {
    uint32_t now = millis();
    // Read external tactile switch (GPIO 3) or onboard BOOT button (GPIO 9)
    bool rawPressed = (digitalRead(PIN_BUTTON) == LOW || digitalRead(PIN_BOOT_BTN) == LOW);

    static bool buttonWasPressed = false;
    static uint32_t lastDebounceMs = 0;
    static uint32_t lastButtonTriggerMs = 0;

    if (rawPressed != buttonWasPressed) {
        if (now - lastDebounceMs > 35) { // 35ms debounce window
            buttonWasPressed = rawPressed;
            lastDebounceMs = now;

            if (buttonWasPressed) { // Pressed down (falling edge)
                if (now - lastButtonTriggerMs > 2500) { // 2.5s cooldown lockout
                    lastButtonTriggerMs = now;
                    Serial.println("\n[BUTTON] >>> AI Vision Push Button Pressed! Signaling Camera... <<<");

                    // Immediate tactile and acoustic feedback (60ms chirp + haptic tap)
                    setBuzzer(true, PLEASANT_ALERT_TONE_HZ);
                    setMotorPwm(200);
                    delay(60);
                    setBuzzer(false);
                    setMotorPwm(0);

                    if (bleClientConnected) {
                        blePrintln(">>> [AI VISION] Camera capture triggered via Physical Push Button <<<");
                    }
                    triggerCameraSnapshot();
                } else {
                    Serial.println("[BUTTON] Press ignored: Capture sequence in progress.");
                }
            }
        }
    }
}

// ============================================================================
// Hardware Camera Interconnect & BLE Image Stream
// ============================================================================
static uint8_t camImgBuf[40960];
constexpr size_t BLE_RAW_CHUNK_SIZE = 180;

void triggerCameraSnapshot() {
    captureAndStreamImageToBle();
}

void captureAndStreamImageToBle() {
    Serial.println("\n[AI VISION] Capture initiated. Signaling ESP32-CAM...");
    
    // Clear any residual bytes in CamSerial buffer
    while (CamSerial.available()) {
        CamSerial.read();
    }

    // 1. Send clean serial command to ESP32-CAM (flush ensures bytes leave UART buffer)
    CamSerial.println("CAPTURE");
    CamSerial.flush();

    // 2. Wait for Start-of-Frame magic header: 0xAA 0x55 0x49 0x4D ("AA55IM")
    uint32_t startWait = millis();
    bool headerFound = false;
    uint8_t magicState = 0;
    String diagBytes = "";

    while (millis() - startWait < 4500) {
        if (CamSerial.available()) {
            uint8_t b = CamSerial.read();
            if (diagBytes.length() < 160) {
                if (b >= 32 && b <= 126) diagBytes += (char)b;
                else diagBytes += "[" + String(b, HEX) + "]";
            }
            if (magicState == 0 && b == 0xAA) magicState = 1;
            else if (magicState == 1 && b == 0x55) magicState = 2;
            else if (magicState == 2 && b == 0x49) magicState = 3;
            else if (magicState == 3 && b == 0x4D) {
                headerFound = true;
                break;
            } else {
                magicState = (b == 0xAA) ? 1 : 0;
            }
        } else {
            delay(2);
        }
    }

    if (!headerFound) {
        Serial.printf("[AI VISION] Camera capture timeout: header not received. (Bytes read: %s)\n",
                      diagBytes.length() > 0 ? diagBytes.c_str() : "None/Silent");
        if (bleClientConnected) {
            blePrintln("IMG:ERROR:CAMERA_TIMEOUT");
        }
        return;
    }

    // 3. Read metadata: length (4 bytes), width (2 bytes), height (2 bytes)
    uint8_t meta[8];
    size_t metaRead = 0;
    startWait = millis();
    while (metaRead < 8 && millis() - startWait < 1000) {
        if (CamSerial.available()) {
            meta[metaRead++] = CamSerial.read();
        } else {
            delay(1);
        }
    }

    if (metaRead < 8) {
        Serial.println("[AI VISION] Camera read error: incomplete header metadata.");
        if (bleClientConnected) blePrintln("IMG:ERROR:INCOMPLETE_HEADER");
        return;
    }

    uint32_t imgLen = (uint32_t)meta[0] | ((uint32_t)meta[1] << 8) |
                      ((uint32_t)meta[2] << 16) | ((uint32_t)meta[3] << 24);
    uint16_t imgW = (uint16_t)meta[4] | ((uint16_t)meta[5] << 8);
    uint16_t imgH = (uint16_t)meta[6] | ((uint16_t)meta[7] << 8);

    Serial.printf("[AI VISION] Incoming image from CAM: %u bytes (%dx%d)\n", imgLen, imgW, imgH);

    if (imgLen == 0 || imgLen > sizeof(camImgBuf)) {
        Serial.printf("[AI VISION] Invalid image length: %u\n", imgLen);
        if (bleClientConnected) blePrintln("IMG:ERROR:IMAGE_TOO_LARGE");
        return;
    }

    // 4. Read payload bytes
    size_t bytesRead = 0;
    startWait = millis();
    while (bytesRead < imgLen && millis() - startWait < 3500) {
        int avail = CamSerial.available();
        if (avail > 0) {
            size_t toRead = avail;
            if (bytesRead + toRead > imgLen) toRead = imgLen - bytesRead;
            CamSerial.readBytes((char*)&camImgBuf[bytesRead], toRead);
            bytesRead += toRead;
        } else {
            delay(1);
        }
    }

    if (bytesRead < imgLen) {
        Serial.printf("[AI VISION] Incomplete payload: %u / %u bytes\n", bytesRead, imgLen);
        if (bleClientConnected) blePrintln("IMG:ERROR:INCOMPLETE_PAYLOAD");
        return;
    }

    // Read 2-byte checksum and 2-byte footer (0x55 0xAA)
    uint8_t footer[4];
    size_t footerRead = 0;
    startWait = millis();
    while (footerRead < 4 && millis() - startWait < 500) {
        if (CamSerial.available()) {
            footer[footerRead++] = CamSerial.read();
        } else {
            delay(1);
        }
    }

    Serial.println("[AI VISION] Image successfully buffered in ESP32-C3 RAM!");

    // 5. Stream over BLE to Phone
    if (!bleClientConnected) {
        Serial.println("[AI VISION] Phone not connected via BLE. Skipping wireless stream.");
        return;
    }

    size_t totalChunks = (imgLen + BLE_RAW_CHUNK_SIZE - 1) / BLE_RAW_CHUNK_SIZE;
    Serial.printf("[AI VISION] Streaming to Phone over BLE NUS: %u bytes in %u chunks...\n", imgLen, totalChunks);

    blePrintln("IMG:START:" + String(imgLen) + ":" + String(totalChunks));
    delay(20);

    for (size_t c = 0; c < totalChunks; c++) {
        size_t offset = c * BLE_RAW_CHUNK_SIZE;
        size_t chunkSize = (offset + BLE_RAW_CHUNK_SIZE <= imgLen) ? BLE_RAW_CHUNK_SIZE : (imgLen - offset);
        String b64 = base64EncodeChunk(&camImgBuf[offset], chunkSize);
        String packet = "IMG:CHUNK:" + String(c) + ":" + b64;
        blePrintln(packet);
        delay(16); // Allow BLE radio queue to flush
    }

    blePrintln("IMG:END");
    Serial.println("\n=======================================================");
    Serial.printf("[AI VISION SUCCESS] Photo captured and transferred successfully!\n");
    Serial.printf("  * Image size: %u bytes (%dx%d)\n", imgLen, imgW, imgH);
    Serial.printf("  * Transferred: %u BLE chunks to phone\n", totalChunks);
    Serial.println("=======================================================\n");
}

// ============================================================================
// Motor & Buzzer Outputs
// ============================================================================
void setMotorPwm(uint8_t duty) {
    uint8_t output = motorOutputDuty(duty, MOTOR_ACTIVE_LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_MOTOR, output);
#else
    ledcWrite(MOTOR_CHANNEL, output);
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

    // 50% duty cycle PWM generates maximum acoustic power with rich low frequency
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
    // Configure target MAC address before BLE stack initializes so the Android companion app
    // (which connects directly to 48:F6:EE:17:C1:78) can connect instantly.
    // ESP32 BT MAC = Base MAC + 2, so Base MAC ending in 0x76 yields BT MAC ending in 0x78.
    uint8_t targetBaseMac[6] = {0x48, 0xF6, 0xEE, 0x17, 0xC1, 0x76};
    esp_base_mac_addr_set(targetBaseMac);

    // Motor initial off levels
    pinMode(PIN_MOTOR, OUTPUT);
    digitalWrite(PIN_MOTOR, MOTOR_ACTIVE_LOW ? HIGH : LOW);

    // Camera Trigger line (Default HIGH)
    pinMode(PIN_CAM_TRIG, OUTPUT);
    digitalWrite(PIN_CAM_TRIG, HIGH);

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // AI Vision Push Button (Active LOW with internal pull-up)
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

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
    // 200 Hz carrier frequency optimal for miniature DC brushed vibration motors
    // (Prevents inductive choke and capacitor bypass at high frequencies)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    bool motorOk = ledcAttachChannel(PIN_MOTOR, MOTOR_PWM_FREQ_HZ, 8, MOTOR_CHANNEL);
    bool buzzOk = ledcAttachChannel(PIN_BUZZER, PLEASANT_ALERT_TONE_HZ, 8, BUZZER_CHANNEL);
    Serial.printf("[LEDC] Motor (GPIO %d, %uHz): %s\n", PIN_MOTOR, MOTOR_PWM_FREQ_HZ, motorOk ? "OK" : "FAILED");
    Serial.printf("[LEDC] Low-frequency buzzer (GPIO %d, %uHz): %s\n", PIN_BUZZER, PLEASANT_ALERT_TONE_HZ, buzzOk ? "OK" : "FAILED");
#else
    ledcSetup(MOTOR_CHANNEL, MOTOR_PWM_FREQ_HZ, 8);
    ledcAttachPin(PIN_MOTOR, MOTOR_CHANNEL);
    ledcSetup(BUZZER_CHANNEL, PLEASANT_ALERT_TONE_HZ, 8);
    ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
#endif
    setMotorPwm(0);
    setBuzzer(false);

    // Start quietly. Manual Serial/BLE motor diagnostics remain available.

    // Initialize I2C Bus (SDA=GPIO 4, SCL=GPIO 5)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(40);

    scanI2C();
    tryInitMPU(true);
    tryInitTOF(true);

    // Initialize Camera UART link (ESP32-C3 <-> ESP32-CAM)
    pinMode(PIN_CAM_TX, OUTPUT);
    digitalWrite(PIN_CAM_TX, HIGH); // Active LOW pulse, idle HIGH
    CamSerial.begin(115200, SERIAL_8N1, PIN_CAM_RX, PIN_CAM_TX);
    Serial.printf("[INIT] Camera UART link on RX: GPIO %d, TX: GPIO %d @ 115200 baud\n", PIN_CAM_RX, PIN_CAM_TX);

    initBLE();

    Serial.println("\n[System] All modules configured. Running intelligent cane loop...\n");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    uint32_t now = millis();

    // Check physical push button (GPIO 3 or onboard BOOT button GPIO 9)
    checkPushButton();

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

        // Serial Command Monitor (Bench testing: type 'c', 'button', 'motor', 'buzz')
        if (Serial.available()) {
            String rawCmd = Serial.readStringUntil('\n');
            rawCmd.trim();
            if (rawCmd.startsWith("cam ") || rawCmd.startsWith("CAM ")) {
                String sub = rawCmd.substring(4);
                Serial.printf("\n[C3->CAM BRIDGE] Sending '%s' to ESP32-CAM...\n", sub.c_str());
                CamSerial.println(sub);
                CamSerial.flush();
                uint32_t w = millis();
                while (millis() - w < 1500) {
                    while (CamSerial.available()) {
                        Serial.write(CamSerial.read());
                    }
                    delay(2);
                }
                Serial.println("\n[C3->CAM BRIDGE] End of response.\n");
                return;
            }
            String cmd = rawCmd;
            cmd.toLowerCase();
            if (cmd == "c" || cmd == "cam" || cmd == "capture" || cmd == "describe" || cmd == "button") {
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
            } else if (cmd == "b" || cmd == "buzz" || cmd == "buzzer") {
                Serial.println("[TEST] Testing buzzer for 400ms at 520 Hz...");
                setBuzzer(true, PLEASANT_ALERT_TONE_HZ);
                delay(400);
                setBuzzer(false);
                Serial.println("[TEST] Buzzer test complete.");
            } else if (cmd == "m" || cmd == "motor" || cmd == "test") {
                Serial.println("\n[TEST] === Haptic Motor Diagnostic Sequence Started ===");
                Serial.println("[TEST] Testing motor (GPIO 2) for 500ms at full duty...");
                setMotorPwm(255);
                delay(500);
                setMotorPwm(0);
                Serial.println("[TEST] === Haptic Motor Diagnostic Sequence Complete ===\n");
                if (bleClientConnected) {
                    blePrintln("[TEST] Motor diagnostic sequence complete.");
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
        motorEnvelope.update(now, 0, 0, 0);
        beepEnvelope.update(now, 0, 0, 0);
        currentMotorPwm = 0;
        setMotorPwm(0);
        buzzerState = false;
        setBuzzer(false);
    } else {
        // The motor pulses gently at long range, then runs continuously nearby.
        if (feedback.motorPulseOnMs > 0) {
            bool on = motorEnvelope.update(now, feedback.motorPulseOnMs,
                                           feedback.motorPulseOffMs, 1);
            currentMotorPwm = on ? feedback.motorDuty : 0;
        } else {
            motorEnvelope.update(now, 0, 0, 0);
            currentMotorPwm = feedback.motorDuty;
        }

        setMotorPwm(currentMotorPwm);

        // Short chirps retain urgency without a near-continuous alarm.
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

        uint8_t vibPercent = (uint8_t)((currentMotorPwm / 255.0f) * 100.0f);

        char telemLine[230];
        snprintf(telemLine, sizeof(telemLine),
                 "[CANE-C3] Dist:%s | ToF:%s | US:%s | Tilt:%s | Fall:%d | Vib:%3d%% (Motor:%3d) | Buzz:%s | %s",
                 distBuf,
                 tofBuf,
                 usBuf,
                 tiltBuf,
                 fallAlert ? 1 : 0,
                 vibPercent,
                 currentMotorPwm,
                 buzzerState ? "ON" : "OFF",
                 alertMsg);

        Serial.println(telemLine);

        if (bleClientConnected) {
            blePrintln(telemLine);
        }
    }
}
