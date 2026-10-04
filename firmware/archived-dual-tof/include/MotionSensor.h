#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "config.h"

/**
 * @brief 6-Axis IMU (MPU6050) driver for orientation tracking and fall detection.
 */
class MotionSensor {
public:
    MotionSensor();

    /**
     * @brief Initialize MPU6050 over I2C.
     * @param wire Reference to TwoWire bus.
     * @return true if detected and initialized.
     */
    bool begin(TwoWire &wire = Wire);

    /**
     * @brief Read accelerometer and gyroscope, update tilt and fall state machines.
     * @return true if updated.
     */
    bool update();

    // Orientation & Acceleration Accessors
    float getTiltAngleDeg() const { return _tiltAngleDeg; }
    float getAccelMagnitudeG() const { return _accelMagG; }
    bool isCaneHorizontal() const { return _tiltAngleDeg >= FALL_TILT_CRITICAL_DEG; }
    bool isFallAlertActive() const { return _fallDetected; }
    void clearFallAlert() { _fallDetected = false; _fallStage = FALL_IDLE; }

    sensors_event_t getAccelEvent() const { return _a; }
    sensors_event_t getGyroEvent() const { return _g; }

private:
    Adafruit_MPU6050 _mpu;
    sensors_event_t _a, _g, _temp;
    float _accelMagG;
    float _tiltAngleDeg;
    bool _initialized;
    bool _fallDetected;
    uint32_t _lastUpdateMs;

    // Fall detection internal state machine
    enum FallStage {
        FALL_IDLE,
        FALL_FREEFALL_DETECTED,
        FALL_IMPACT_DETECTED,
        FALL_CHECK_IMMOBILITY
    };
    FallStage _fallStage;
    uint32_t _freefallTimestamp;
    uint32_t _impactTimestamp;
};

#endif // MOTION_SENSOR_H
