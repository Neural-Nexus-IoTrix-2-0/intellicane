#ifndef FORWARD_SENSOR_H
#define FORWARD_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include "config.h"

/**
 * @brief Forward-facing Time-of-Flight (VL53L1X) obstacle sensor driver.
 *
 * Provides non-blocking ranging up to 4m with configurable distance modes.
 */
class ForwardSensor {
public:
    ForwardSensor();

    /**
     * @brief Initialize VL53L1X sensor on the specified I2C bus.
     * @param wire Reference to TwoWire instance.
     * @return true if initialized successfully, false otherwise.
     */
    bool begin(TwoWire &wire = Wire);

    /**
     * @brief Non-blocking update. Checks if new ranging data is available.
     * @return true if new measurement was read, false otherwise.
     */
    bool update();

    // Accessors
    uint16_t getDistanceMm() const { return _distanceMm; }
    float getDistanceCm() const { return _distanceMm / 10.0f; }
    bool isValid() const { return _isValid; }
    bool isObstacleDetected() const;
    bool isCritical() const;
    uint8_t getRangeStatus() const { return _rangeStatus; }
    uint32_t getLastMeasurementTime() const { return _lastMeasurementMs; }

private:
    VL53L1X _sensor;
    uint16_t _distanceMm;
    uint8_t _rangeStatus;
    bool _isValid;
    bool _initialized;
    uint32_t _lastMeasurementMs;
};

#endif // FORWARD_SENSOR_H
