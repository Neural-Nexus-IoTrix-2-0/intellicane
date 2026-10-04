#pragma once

#include <stdint.h>
#include <math.h>

struct ProximityFeedback {
    uint8_t motorDuty;
    uint16_t beepOnMs;
    uint16_t beepOffMs;
    uint16_t toneHz;
};

inline ProximityFeedback feedbackForDistance(float distanceCm) {
    // No echo, invalid readings and the 60 cm boundary must not drive the motor.
    if (!isfinite(distanceCm) || distanceCm <= 0.0f || distanceCm >= 60.0f) {
        return {0, 0, 0, 0};
    }
    float proximity = (60.0f - distanceCm) / 50.0f;
    if (proximity > 1.0f) proximity = 1.0f;
    // Minimum duty helps overcome motor starting friction. Full strength at 10 cm.
    return {
        static_cast<uint8_t>(100 + proximity * 155),
        static_cast<uint16_t>(60 + proximity * 140),
        static_cast<uint16_t>(740 - proximity * 700),
        static_cast<uint16_t>(1200 + proximity * 1600)
    };
}

inline uint8_t motorOutputDuty(uint8_t strength, bool activeLow) {
    return activeLow ? 255 - strength : strength;
}

class BeepEnvelope {
public:
    bool update(uint32_t now, uint16_t onMs, uint16_t offMs, uint8_t mode) {
        if (mode == 0) {
            mode_ = 0;
            return false;
        }
        if (mode != mode_) {
            mode_ = mode;
            startMs_ = now; // Every new alert starts audibly, without stale state.
        }
        uint32_t elapsed = now - startMs_;
        if (elapsed >= static_cast<uint32_t>(onMs) + offMs) {
            startMs_ = now;
            elapsed = 0;
        }
        return elapsed < onMs;
    }
private:
    uint8_t mode_ = 0;
    uint32_t startMs_ = 0;
};
