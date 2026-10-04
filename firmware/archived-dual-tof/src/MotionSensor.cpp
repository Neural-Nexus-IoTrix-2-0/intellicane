#include "MotionSensor.h"
#include <math.h>

MotionSensor::MotionSensor()
    : _accelMagG(1.0f),
      _tiltAngleDeg(0.0f),
      _initialized(false),
      _fallDetected(false),
      _lastUpdateMs(0),
      _fallStage(FALL_IDLE),
      _freefallTimestamp(0),
      _impactTimestamp(0) {}

bool MotionSensor::begin(TwoWire &wire) {
    if (!_mpu.begin(0x68, &wire)) {
        Serial.println("[MotionSensor] ERROR: Could not find MPU6050 chip at 0x68!");
        _initialized = false;
        return false;
    }

    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    _initialized = true;
    Serial.println("[MotionSensor] MPU6050 initialized successfully (Range: ±8G, Filter: 21Hz)");
    return true;
}

bool MotionSensor::update() {
    if (!_initialized) return false;

    uint32_t now = millis();
    if (now - _lastUpdateMs < INTERVAL_MOTION_SENSOR_MS) {
        return false;
    }
    _lastUpdateMs = now;

    if (!_mpu.getEvent(&_a, &_g, &_temp)) {
        return false;
    }

    // Convert acceleration vector magnitude to standard Earth gravity (g)
    float rawMag = sqrtf((_a.acceleration.x * _a.acceleration.x) +
                         (_a.acceleration.y * _a.acceleration.y) +
                         (_a.acceleration.z * _a.acceleration.z));
    _accelMagG = rawMag / 9.80665f;

    // Calculate cane inclination/tilt relative to vertical gravity vector
    // Standard upright cane has gravity along the primary shaft axis (assumed Z or Y depending on orientation)
    // Here we compute angle deviation from vertical:
    // If the cane is standing vertically, one axis ~9.8 m/s^2 and others ~0.
    // If lying flat on the floor, tilt reaches ~90 degrees.
    if (rawMag > 0.1f) {
        // Vertical axis alignment assumed to be Z in standard board placement
        float cosTilt = fabsf(_a.acceleration.z) / rawMag;
        if (cosTilt > 1.0f) cosTilt = 1.0f;
        _tiltAngleDeg = acosf(cosTilt) * (180.0f / (float)M_PI);
    }

    // ========================================================================
    // Multi-stage Fall Detection Algorithm:
    // Stage 1: Freefall (G drops below 0.4g)
    // Stage 2: Impact spike (G surges above 2.5g within 400ms)
    // Stage 3: Immobility (cane stays tilted > 70 deg and stationary for 3s)
    // ========================================================================
    switch (_fallStage) {
        case FALL_IDLE:
            if (_accelMagG < FALL_FREEFALL_G_THRESH) {
                _fallStage = FALL_FREEFALL_DETECTED;
                _freefallTimestamp = now;
            } else if (_accelMagG > FALL_IMPACT_G_THRESH) {
                // Direct hard drop/impact without extended freefall
                _fallStage = FALL_IMPACT_DETECTED;
                _impactTimestamp = now;
            }
            break;

        case FALL_FREEFALL_DETECTED:
            if (_accelMagG > FALL_IMPACT_G_THRESH) {
                _fallStage = FALL_IMPACT_DETECTED;
                _impactTimestamp = now;
            } else if (now - _freefallTimestamp > 500) {
                // Freefall timeout without impact -> reset
                _fallStage = FALL_IDLE;
            }
            break;

        case FALL_IMPACT_DETECTED:
            if (now - _impactTimestamp > 300) {
                // Cane has landed; start immobility evaluation
                _fallStage = FALL_CHECK_IMMOBILITY;
            }
            break;

        case FALL_CHECK_IMMOBILITY:
            if (isCaneHorizontal()) {
                if (now - _impactTimestamp >= FALL_IMMOBILITY_TIME_MS) {
                    _fallDetected = true;
                    Serial.println("[MotionSensor] CRITICAL: Fall detected! Cane is stationary on ground.");
                }
            } else {
                // User picked up the cane or cane is upright -> false alarm / recovered
                _fallStage = FALL_IDLE;
            }
            break;
    }

    return true;
}
