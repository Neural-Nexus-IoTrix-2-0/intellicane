#ifndef DOWNWARD_SENSOR_H
#define DOWNWARD_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

#if (DOWNWARD_SENSOR_TYPE == SENSOR_TYPE_VL53L0X)
#include <VL53L0X.h>
#endif

/**
 * @brief Downward-facing ground / drop-off sensor driver.
 *
 * Configurable for either Ultrasonic (HC-SR04/US-015) or VL53L0X ToF laser.
 * Tracks ground baseline distance and detects descending stairs, curbs, and drop-offs.
 */
class DownwardSensor {
public:
    DownwardSensor();

    /**
     * @brief Initialize downward sensor hardware.
     * @param wire Reference to TwoWire instance (for ToF mode).
     * @return true if initialized, false otherwise.
     */
    bool begin(TwoWire &wire = Wire);

    /**
     * @brief Non-blocking update cycle.
     * @return true if a fresh reading was acquired, false otherwise.
     */
    bool update();

    // Baseline management & calibration
    void calibrateBaseline(uint8_t sampleCount = 10);
    void setBaselineCm(float baselineCm) { _baselineCm = baselineCm; }
    float getBaselineCm() const { return _baselineCm; }

    // Ranging Accessors
    float getDistanceCm() const { return _currentDistanceCm; }
    bool isValid() const { return _isValid; }
    bool isDropOffDetected() const;

private:
    float _currentDistanceCm;
    float _baselineCm;
    bool _isValid;
    bool _initialized;
    uint32_t _lastMeasureMs;

#if (DOWNWARD_SENSOR_TYPE == SENSOR_TYPE_VL53L0X)
    VL53L0X _tofSensor;
#else
    // Ultrasonic timing state
    float readUltrasonicCm();
#endif
};

#endif // DOWNWARD_SENSOR_H
