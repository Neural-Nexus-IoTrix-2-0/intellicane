#include "BuzzerAlert.h"

BuzzerAlert::BuzzerAlert()
    : _mode(MODE_SILENT),
      _stateStartMs(0),
      _buzzerActive(false) {}

void BuzzerAlert::begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    _buzzerActive = false;
    Serial.println("[BuzzerAlert] Buzzer initialized on GPIO 26");
}

void BuzzerAlert::setBuzzerState(bool active) {
    _buzzerActive = active;
    digitalWrite(PIN_BUZZER, active ? HIGH : LOW);
}

void BuzzerAlert::setMode(BuzzerMode mode) {
    if (_mode != mode) {
        _mode = mode;
        _stateStartMs = millis();
        if (_mode == MODE_SILENT) {
            setBuzzerState(false);
        }
    }
}

void BuzzerAlert::playBootBeep() {
    _mode = MODE_BOOT_BEEP;
    _stateStartMs = millis();
    setBuzzerState(true);
}

void BuzzerAlert::silence() {
    setMode(MODE_SILENT);
}

void BuzzerAlert::update() {
    uint32_t now = millis();

    switch (_mode) {
        case MODE_SILENT:
            setBuzzerState(false);
            break;

        case MODE_BOOT_BEEP:
            if (now - _stateStartMs < 120) {
                setBuzzerState(true);
            } else {
                setBuzzerState(false);
                _mode = MODE_SILENT;
            }
            break;

        case MODE_CRITICAL_OBSTACLE: {
            // Rapid urgent beeping: 80ms ON, 80ms OFF
            uint32_t elapsed = (now - _stateStartMs) % 160;
            setBuzzerState(elapsed < 80);
            break;
        }

        case MODE_FALL_ALARM: {
            // Loud alternating siren pattern: 250ms ON, 150ms OFF
            uint32_t elapsed = (now - _stateStartMs) % 400;
            setBuzzerState(elapsed < 250);
            break;
        }
    }
}
