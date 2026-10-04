#ifndef CANE_CONTROLLER_H
#define CANE_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "ForwardSensor.h"
#include "DownwardSensor.h"
#include "MotionSensor.h"
#include "HapticFeedback.h"
#include "BuzzerAlert.h"

/**
 * @brief Master safety controller coordinating all sensors, safety arbitration logic,
 *        actuator patterns, and serial telemetry.
 */
class CaneController {
public:
    CaneController();

    /**
     * @brief Initialize all hardware subsystems and perform self-test.
     */
    void begin();

    /**
     * @brief Main non-blocking loop update.
     */
    void update();

    // Diagnostics & Sensor access
    const ForwardSensor& getForwardSensor() const { return _forwardSensor; }
    const DownwardSensor& getDownwardSensor() const { return _downwardSensor; }
    const MotionSensor& getMotionSensor() const { return _motionSensor; }
    const HapticFeedback& getHapticFeedback() const { return _haptic; }
    const BuzzerAlert& getBuzzerAlert() const { return _buzzer; }

private:
    ForwardSensor   _forwardSensor;
    DownwardSensor  _downwardSensor;
    MotionSensor    _motionSensor;
    HapticFeedback  _haptic;
    BuzzerAlert     _buzzer;

    uint32_t _lastForwardUpdateMs;
    uint32_t _lastDownwardUpdateMs;
    uint32_t _lastMotionUpdateMs;
    uint32_t _lastActuatorUpdateMs;
    uint32_t _lastTelemetryMs;
    uint32_t _lastLedBlinkMs;
    bool _ledState;

    void updateSensors();
    void evaluateSafetyAndActuate();
    void printTelemetry();
    void handleStatusLed(bool hazardActive);
    void checkSosButton();
};

#endif // CANE_CONTROLLER_H
