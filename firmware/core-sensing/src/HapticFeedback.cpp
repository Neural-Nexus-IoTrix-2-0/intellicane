#include "HapticFeedback.h"
#include <esp_arduino_version.h>

HapticFeedback::HapticFeedback()
    : _pattern(PATTERN_OFF),
      _currentDuty(0),
      _targetDistanceCm(999.0f),
      _patternStartMs(0) {}

void HapticFeedback::begin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(PIN_VIBRATION_PWM, HAPTIC_PWM_FREQ, HAPTIC_PWM_RESOLUTION);
#else
    ledcSetup(HAPTIC_PWM_CHANNEL, HAPTIC_PWM_FREQ, HAPTIC_PWM_RESOLUTION);
    ledcAttachPin(PIN_VIBRATION_PWM, HAPTIC_PWM_CHANNEL);
#endif
    applyPwmDuty(0);
    Serial.println("[HapticFeedback] LEDC PWM initialized on GPIO 25 (5kHz, 8-bit)");
}

void HapticFeedback::applyPwmDuty(uint8_t duty) {
    _currentDuty = duty;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(PIN_VIBRATION_PWM, duty);
#else
    ledcWrite(HAPTIC_PWM_CHANNEL, duty);
#endif
}

uint8_t HapticFeedback::calculateProportionalDuty(float distCm) {
    if (distCm > FORWARD_MAX_ALERT_DIST_CM) {
        return 0; // Clear zone
    }

    if (distCm <= FORWARD_CRITICAL_DIST_CM) {
        return HAPTIC_PWM_MAX; // 100% duty at <= 30cm
    }

    // Normalized progress from max distance down to critical distance [0.0 .. 1.0]
    float norm = (FORWARD_MAX_ALERT_DIST_CM - distCm) / (FORWARD_MAX_ALERT_DIST_CM - FORWARD_CRITICAL_DIST_CM);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    // Linear mapping between min spin threshold and max duty
    uint8_t duty = (uint8_t)(HAPTIC_PWM_MIN_SPIN + norm * (HAPTIC_PWM_MAX - HAPTIC_PWM_MIN_SPIN));
    return duty;
}

void HapticFeedback::setProportionalDistance(float distanceCm) {
    _pattern = PATTERN_PROPORTIONAL;
    _targetDistanceCm = distanceCm;
}

void HapticFeedback::triggerDropOffPattern() {
    if (_pattern != PATTERN_DROP_OFF) {
        _pattern = PATTERN_DROP_OFF;
        _patternStartMs = millis();
    }
}

void HapticFeedback::triggerFallPattern() {
    if (_pattern != PATTERN_FALL_ALERT) {
        _pattern = PATTERN_FALL_ALERT;
        _patternStartMs = millis();
    }
}

void HapticFeedback::turnOff() {
    _pattern = PATTERN_OFF;
    applyPwmDuty(0);
}

void HapticFeedback::update() {
    uint32_t now = millis();

    switch (_pattern) {
        case PATTERN_OFF:
            applyPwmDuty(0);
            break;

        case PATTERN_PROPORTIONAL: {
            uint8_t duty = calculateProportionalDuty(_targetDistanceCm);
            applyPwmDuty(duty);
            break;
        }

        case PATTERN_DROP_OFF: {
            // Double burst signature:
            // 0..150ms: ON (High power)
            // 150..230ms: OFF
            // 230..380ms: ON (High power)
            // 380..650ms: OFF
            uint32_t elapsed = (now - _patternStartMs) % DROP_OFF_CYCLE_PERIOD_MS;
            if (elapsed < DROP_OFF_PULSE_ON_1_MS) {
                applyPwmDuty(240);
            } else if (elapsed < (DROP_OFF_PULSE_ON_1_MS + DROP_OFF_PULSE_OFF_1_MS)) {
                applyPwmDuty(0);
            } else if (elapsed < (DROP_OFF_PULSE_ON_1_MS + DROP_OFF_PULSE_OFF_1_MS + DROP_OFF_PULSE_ON_2_MS)) {
                applyPwmDuty(240);
            } else {
                applyPwmDuty(0);
            }
            break;
        }

        case PATTERN_FALL_ALERT: {
            // Pulse: 200ms ON / 200ms OFF
            uint32_t elapsed = (now - _patternStartMs) % 400;
            if (elapsed < 200) {
                applyPwmDuty(255);
            } else {
                applyPwmDuty(0);
            }
            break;
        }
    }
}
