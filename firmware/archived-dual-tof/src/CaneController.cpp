#include "CaneController.h"

CaneController::CaneController()
    : _lastForwardUpdateMs(0),
      _lastDownwardUpdateMs(0),
      _lastMotionUpdateMs(0),
      _lastActuatorUpdateMs(0),
      _lastTelemetryMs(0),
      _lastLedBlinkMs(0),
      _ledState(false) {}

void CaneController::begin() {
    Serial.println("\n=======================================================");
    Serial.println("  INTELLIGENT CANE — PHASE 1 CORE SENSING BOOTING");
    Serial.println("  Neural-Nexus IoTrix 2.0 (Track A Embedded IoT)");
    Serial.println("=======================================================");

    // Initialize Status LED & SOS button
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, HIGH);
    pinMode(PIN_BUTTON_SOS, INPUT_PULLUP);

    // Initialize Actuators first so audio/haptic cues are ready
    _haptic.begin();
    _buzzer.begin();
    _buzzer.playBootBeep();

    // Initialize Shared I2C Bus (Fast Mode 400kHz)
    Serial.printf("[System] Initializing I2C bus (SDA=GPIO%d, SCL=GPIO%d @ 400kHz)...\n", PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    delay(50);

    // Initialize Forward ToF Sensor (VL53L1X)
    bool fwdOk = _forwardSensor.begin(Wire);
    if (!fwdOk) {
        Serial.println("[System] WARNING: Forward ToF sensor failed to start!");
    }

    // Initialize Downward Sensor (Ultrasonic or VL53L0X)
    bool downOk = _downwardSensor.begin(Wire);
    if (!downOk) {
        Serial.println("[System] WARNING: Downward sensor failed to start!");
    }

    // Initialize MPU6050 Motion Sensor
    bool motionOk = _motionSensor.begin(Wire);
    if (!motionOk) {
        Serial.println("[System] WARNING: MPU6050 IMU failed to start!");
    }

    Serial.println("-------------------------------------------------------");
    Serial.printf("  Forward ToF:    [%s]\n", fwdOk ? "OK (VL53L1X)" : "FAIL");
    Serial.printf("  Downward:       [%s]\n", downOk ? "OK" : "FAIL");
    Serial.printf("  MPU6050 IMU:    [%s]\n", motionOk ? "OK" : "FAIL");
    Serial.printf("  Haptic Motor:   [OK (GPIO%d LEDC CH%d)]\n", PIN_VIBRATION_PWM, HAPTIC_PWM_CHANNEL);
    Serial.printf("  Piezo Buzzer:   [OK (GPIO%d)]\n", PIN_BUZZER);
    Serial.println("=======================================================\n");

    digitalWrite(PIN_LED_STATUS, LOW);
}

void CaneController::updateSensors() {
    uint32_t now = millis();

    // 1. Update Forward ToF Sensor
    if (now - _lastForwardUpdateMs >= INTERVAL_FORWARD_SENSOR_MS) {
        _lastForwardUpdateMs = now;
        _forwardSensor.update();
    }

    // 2. Update Downward Sensor
    if (now - _lastDownwardUpdateMs >= INTERVAL_DOWNWARD_SENSOR_MS) {
        _lastDownwardUpdateMs = now;
        _downwardSensor.update();
    }

    // 3. Update MPU6050 IMU
    if (now - _lastMotionUpdateMs >= INTERVAL_MOTION_SENSOR_MS) {
        _lastMotionUpdateMs = now;
        _motionSensor.update();
    }
}

void CaneController::evaluateSafetyAndActuate() {
    bool hazardActive = false;

    // ========================================================================
    // PRIORITY 1: Fall / Impact Alarm (Highest Safety Precedence)
    // ========================================================================
    if (_motionSensor.isFallAlertActive()) {
        _haptic.triggerFallPattern();
        _buzzer.setMode(BuzzerAlert::MODE_FALL_ALARM);
        hazardActive = true;
    }
    // ========================================================================
    // PRIORITY 2: Drop-off Detected (Stairs down / Curbs / Open drains)
    // ========================================================================
    else if (_downwardSensor.isDropOffDetected()) {
        // Distinct double-burst tactile signature so user halts immediately
        _haptic.triggerDropOffPattern();
        _buzzer.silence(); // tactile emphasis
        hazardActive = true;
    }
    // ========================================================================
    // PRIORITY 3: Forward Obstacle Proximity Ranging
    // ========================================================================
    else if (_forwardSensor.isObstacleDetected()) {
        float fwdDist = _forwardSensor.getDistanceCm();

        if (_forwardSensor.isCritical()) {
            // Very close obstacle (<30cm): Full 100% vibration + urgent audio beep
            _haptic.setProportionalDistance(fwdDist);
            _buzzer.setMode(BuzzerAlert::MODE_CRITICAL_OBSTACLE);
            hazardActive = true;
        } else {
            // Proportional caution / warning zone (30cm .. 150cm)
            _haptic.setProportionalDistance(fwdDist);
            _buzzer.silence();
            hazardActive = (fwdDist < FORWARD_CAUTION_DIST_CM);
        }
    }
    // ========================================================================
    // CLEAR: All sensors indicate safe path ahead
    // ========================================================================
    else {
        _haptic.turnOff();
        _buzzer.silence();
        hazardActive = false;
    }

    // Update non-blocking actuator timers
    _haptic.update();
    _buzzer.update();

    // Manage status LED blinking rate
    handleStatusLed(hazardActive);
}

void CaneController::handleStatusLed(bool hazardActive) {
    uint32_t now = millis();
    uint32_t blinkInterval = hazardActive ? 100 : 1000; // Fast blink on hazard, slow pulse in clear

    if (now - _lastLedBlinkMs >= blinkInterval) {
        _lastLedBlinkMs = now;
        _ledState = !_ledState;
        digitalWrite(PIN_LED_STATUS, _ledState ? HIGH : LOW);
    }
}

void CaneController::checkSosButton() {
    if (digitalRead(PIN_BUTTON_SOS) == LOW) {
        // SOS Button pressed: reset any latched fall alarm or acknowledge alert
        if (_motionSensor.isFallAlertActive()) {
            Serial.println("[Button] SOS button pressed -> Resetting fall alarm.");
            _motionSensor.clearFallAlert();
        }
    }
}

void CaneController::printTelemetry() {
    uint32_t now = millis();
    if (now - _lastTelemetryMs < INTERVAL_TELEMETRY_LOG_MS) {
        return;
    }
    _lastTelemetryMs = now;

    // Formatted telemetry output (human-readable and easy to parse)
    float fwdDist = _forwardSensor.isValid() ? _forwardSensor.getDistanceCm() : -1.0f;
    float downDist = _downwardSensor.isValid() ? _downwardSensor.getDistanceCm() : -1.0f;
    float tilt = _motionSensor.getTiltAngleDeg();
    float gMag = _motionSensor.getAccelMagnitudeG();
    uint8_t vibDuty = _haptic.getCurrentDuty();
    const char* patternStr = "OFF";

    switch (_haptic.getActivePattern()) {
        case HapticFeedback::PATTERN_PROPORTIONAL: patternStr = "PROP"; break;
        case HapticFeedback::PATTERN_DROP_OFF:     patternStr = "DROP_OFF!"; break;
        case HapticFeedback::PATTERN_FALL_ALERT:   patternStr = "FALL!"; break;
        default: break;
    }

    Serial.printf("[TELEM] Fwd: %5.1f cm | Down: %5.1f cm (Base: %.1f) | Tilt: %4.1f deg | G: %4.2f | Haptic: %3d PWM (%s) | Buzz: %d\n",
                  fwdDist,
                  downDist,
                  _downwardSensor.getBaselineCm(),
                  tilt,
                  gMag,
                  vibDuty,
                  patternStr,
                  _buzzer.getMode() != BuzzerAlert::MODE_SILENT ? 1 : 0);
}

void CaneController::update() {
    checkSosButton();
    updateSensors();
    evaluateSafetyAndActuate();
    printTelemetry();
}
