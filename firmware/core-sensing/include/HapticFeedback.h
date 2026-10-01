#ifndef HAPTIC_FEEDBACK_H
#define HAPTIC_FEEDBACK_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief Non-blocking haptic feedback controller managing vibration PWM intensity
 *        and distinctive tactile alarm patterns.
 */
class HapticFeedback {
public:
    enum HapticPattern {
        PATTERN_OFF,
        PATTERN_PROPORTIONAL,   // Steady vibration scaled by forward proximity
        PATTERN_DROP_OFF,       // Double rhythmic burst for ground drop-offs / curbs
        PATTERN_FALL_ALERT      // Fast alert pulse when cane is down
    };

    HapticFeedback();

    /**
     * @brief Initialize LEDC PWM channel and pin assignment.
     */
    void begin();

    /**
     * @brief Set pattern to proportional vibration with target forward distance.
     * @param distanceCm Distance to nearest forward obstacle.
     */
    void setProportionalDistance(float distanceCm);

    /**
     * @brief Trigger drop-off tactile warning signature.
     */
    void triggerDropOffPattern();

    /**
     * @brief Trigger fall alarm vibration pattern.
     */
    void triggerFallPattern();

    /**
     * @brief Turn vibration off.
     */
    void turnOff();

    /**
     * @brief Non-blocking state update. Must be called regularly in loop().
     */
    void update();

    // Accessors
    uint8_t getCurrentDuty() const { return _currentDuty; }
    HapticPattern getActivePattern() const { return _pattern; }

private:
    HapticPattern _pattern;
    uint8_t _currentDuty;
    float _targetDistanceCm;
    uint32_t _patternStartMs;

    void applyPwmDuty(uint8_t duty);
    uint8_t calculateProportionalDuty(float distCm);
};

#endif // HAPTIC_FEEDBACK_H
