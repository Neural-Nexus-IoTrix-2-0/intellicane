#include "ForwardSensor.h"

ForwardSensor::ForwardSensor()
    : _distanceMm(0),
      _rangeStatus(0),
      _isValid(false),
      _initialized(false),
      _lastMeasurementMs(0) {}

bool ForwardSensor::begin(TwoWire &wire) {
    _sensor.setBus(&wire);
    _sensor.setTimeout(500);

    if (!_sensor.init()) {
        Serial.println("[ForwardSensor] ERROR: Failed to detect and initialize VL53L1X!");
        _initialized = false;
        return false;
    }

    // Configure distance mode: Medium provides good balance of range (up to 3m) and ambient light immunity
    _sensor.setDistanceMode(VL53L1X::Medium);
    // Set measurement timing budget (33 ms provides ~30 Hz ranging)
    _sensor.setMeasurementTimingBudget(33000);
    // Start continuous ranging with 33 ms period
    _sensor.startContinuous(33);

    _initialized = true;
    _isValid = false;
    Serial.println("[ForwardSensor] VL53L1X initialized successfully (Continuous Medium Mode @ 30Hz)");
    return true;
}

bool ForwardSensor::update() {
    if (!_initialized) {
        return false;
    }

    if (_sensor.dataReady()) {
        _sensor.read(false); // Read non-blockingly
        _distanceMm = _sensor.ranging_data.range_mm;
        _rangeStatus = _sensor.ranging_data.range_status;

        // VL53L1X range_status == 0 indicates valid target range
        _isValid = (_rangeStatus == 0) && (_distanceMm > (FORWARD_MIN_BLIND_DIST_CM * 10));
        _lastMeasurementMs = millis();
        return true;
    }

    // Check for sensor communication timeout
    if (_sensor.timeoutOccurred()) {
        Serial.println("[ForwardSensor] WARNING: VL53L1X sensor timeout occurred!");
        _isValid = false;
    }

    return false;
}

bool ForwardSensor::isObstacleDetected() const {
    if (!_isValid) return false;
    return getDistanceCm() <= FORWARD_MAX_ALERT_DIST_CM;
}

bool ForwardSensor::isCritical() const {
    if (!_isValid) return false;
    return getDistanceCm() <= FORWARD_CRITICAL_DIST_CM;
}
