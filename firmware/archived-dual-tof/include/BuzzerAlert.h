#ifndef BUZZER_ALERT_H
#define BUZZER_ALERT_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief Non-blocking audible buzzer driver for critical hazard warnings and fall alarms.
 */
class BuzzerAlert {
public:
    enum BuzzerMode {
        MODE_SILENT,
        MODE_CRITICAL_OBSTACLE,  // Fast urgent beeps (<30cm obstacle)
        MODE_FALL_ALARM,         // Continuous repeating emergency chirp
        MODE_BOOT_BEEP           // Single startup confirmation pip
    };

    BuzzerAlert();

    /**
     * @brief Initialize buzzer pin.
     */
    void begin();

    /**
     * @brief Set active buzzer alert mode.
     */
    void setMode(BuzzerMode mode);

    /**
     * @brief Trigger a single short confirmation chirp.
     */
    void playBootBeep();

    /**
     * @brief Silence buzzer.
     */
    void silence();

    /**
     * @brief Non-blocking state update called in loop().
     */
    void update();

    BuzzerMode getMode() const { return _mode; }

private:
    BuzzerMode _mode;
    uint32_t _stateStartMs;
    bool _buzzerActive;

    void setBuzzerState(bool active);
};

#endif // BUZZER_ALERT_H
