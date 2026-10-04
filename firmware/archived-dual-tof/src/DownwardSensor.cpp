#include "DownwardSensor.h"

DownwardSensor::DownwardSensor()
    : _currentDistanceCm(DOWNWARD_BASELINE_NOMINAL_CM),
      _baselineCm(DOWNWARD_BASELINE_NOMINAL_CM),
      _isValid(false),
      _initialized(false),
      _lastMeasureMs(0) {}

bool DownwardSensor::begin(TwoWire &wire) {
#if (DOWNWARD_SENSOR_TYPE == SENSOR_TYPE_VL53L0X)
    // Setup XSHUT pin to control VL53L0X boot and readdressing
    pinMode(PIN_TOF_DOWN_XSHUT, OUTPUT);
    digitalWrite(PIN_TOF_DOWN_XSHUT, LOW); // Hold in reset
    delay(10);
    digitalWrite(PIN_TOF_DOWN_XSHUT, HIGH); // Release reset
    delay(10);

    _tofSensor.setBus(&wire);
    _tofSensor.setTimeout(500);

    if (!_tofSensor.init()) {
        Serial.println("[DownwardSensor] ERROR: Failed to detect and initialize VL53L0X!");
        _initialized = false;
        return false;
    }

    // Change I2C address from 0x29 to 0x30 to avoid conflict with VL53L1X
    _tofSensor.setAddress(0x30);
    _tofSensor.startContinuous(40);
    _initialized = true;
    Serial.println("[DownwardSensor] VL53L0X initialized on I2C address 0x30");
#else
    // Configure Ultrasonic GPIO pins
    pinMode(PIN_US_TRIG, OUTPUT);
    pinMode(PIN_US_ECHO, INPUT);
    digitalWrite(PIN_US_TRIG, LOW);
    _initialized = true;
    Serial.println("[DownwardSensor] Ultrasonic sensor initialized (Trig=18, Echo=5)");
#endif

    calibrateBaseline(5);
    return _initialized;
}

#if (DOWNWARD_SENSOR_TYPE != SENSOR_TYPE_VL53L0X)
float DownwardSensor::readUltrasonicCm() {
    // Generate clean 10us trigger pulse
    digitalWrite(PIN_US_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    // Measure echo pulse duration (timeout ~25000us = ~4.3m max distance)
    unsigned long durationUs = pulseIn(PIN_US_ECHO, HIGH, 25000);

    if (durationUs == 0) {
        // Echo timed out: target is either too far (> 4m) or no reflective surface (drop-off / deep hole)
        return -1.0f;
    }

    // Speed of sound = 343 m/s = 0.0343 cm/us
    // Distance = (duration * 0.0343) / 2
    return (float)durationUs * 0.01715f;
}
#endif

bool DownwardSensor::update() {
    if (!_initialized) return false;

    uint32_t now = millis();
    if (now - _lastMeasureMs < INTERVAL_DOWNWARD_SENSOR_MS) {
        return false;
    }
    _lastMeasureMs = now;

#if (DOWNWARD_SENSOR_TYPE == SENSOR_TYPE_VL53L0X)
    uint16_t distMm = _tofSensor.readRangeContinuousMillimeters();
    if (_tofSensor.timeoutOccurred() || distMm >= 8000) {
        // Out of range reading can represent open drop-off
        _currentDistanceCm = DOWNWARD_MAX_VALID_DIST_CM + 10.0f;
        _isValid = true;
    } else {
        _currentDistanceCm = distMm / 10.0f;
        _isValid = (_currentDistanceCm > 3.0f && _currentDistanceCm < DOWNWARD_MAX_VALID_DIST_CM);
    }
#else
    float dist = readUltrasonicCm();
    if (dist < 0.0f) {
        // Timeout: no ground reflected back -> potential large drop-off or void
        _currentDistanceCm = DOWNWARD_MAX_VALID_DIST_CM + 50.0f;
        _isValid = true;
    } else {
        _currentDistanceCm = dist;
        _isValid = (_currentDistanceCm > 2.0f && _currentDistanceCm <= DOWNWARD_MAX_VALID_DIST_CM);
    }
#endif

    return true;
}

void DownwardSensor::calibrateBaseline(uint8_t sampleCount) {
    float sum = 0.0f;
    uint8_t validSamples = 0;

    Serial.println("[DownwardSensor] Calibrating ground baseline distance...");

    for (uint8_t i = 0; i < sampleCount; ++i) {
#if (DOWNWARD_SENSOR_TYPE == SENSOR_TYPE_VL53L0X)
        uint16_t distMm = _tofSensor.readRangeContinuousMillimeters();
        if (!_tofSensor.timeoutOccurred() && distMm > 50 && distMm < 1500) {
            sum += (distMm / 10.0f);
            validSamples++;
        }
#else
        float d = readUltrasonicCm();
        if (d > 5.0f && d < 120.0f) {
            sum += d;
            validSamples++;
        }
#endif
        delay(40);
    }

    if (validSamples > 0) {
        _baselineCm = sum / validSamples;
        Serial.printf("[DownwardSensor] Baseline calibrated: %.1f cm (from %d samples)\n", _baselineCm, validSamples);
    } else {
        _baselineCm = DOWNWARD_BASELINE_NOMINAL_CM;
        Serial.printf("[DownwardSensor] Warning: Calibration failed; using default: %.1f cm\n", _baselineCm);
    }
}

bool DownwardSensor::isDropOffDetected() const {
    if (!_isValid) {
        return false;
    }

    // Condition 1: Distance reading exceeds baseline ground distance by drop threshold
    if (_currentDistanceCm > (_baselineCm + DROP_OFF_DELTA_THRESHOLD_CM)) {
        return true;
    }

    // Condition 2: Ground reflection lost completely (e.g. step edge or pit)
    if (_currentDistanceCm >= DOWNWARD_MAX_VALID_DIST_CM) {
        return true;
    }

    return false;
}
