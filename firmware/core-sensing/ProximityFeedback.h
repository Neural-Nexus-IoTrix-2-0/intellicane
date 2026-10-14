#pragma once

#include <stdint.h>
#include <math.h>

struct ProximityFeedback {
    uint8_t motorDuty;
    uint16_t beepOnMs;
    uint16_t beepOffMs;
    uint16_t toneHz;
};

enum class GroundHazardType : uint8_t {
    NONE = 0,
    DROP_OFF = 1,  // Stairs down, curb drop, pothole, platform edge
    STEP_UP = 2    // Curb up, low hurdle, raised threshold
};

struct GroundHazardFeedback {
    GroundHazardType type;
    uint8_t motorDuty;      // PWM duty for Motor 2 (0-255)
    uint16_t pulseOnMs;
    uint16_t pulseOffMs;
};

inline ProximityFeedback feedbackForDistance(float distanceCm) {
    // No echo, invalid readings and the 60 cm boundary must not drive the motor.
    if (!isfinite(distanceCm) || distanceCm <= 0.0f || distanceCm >= 60.0f) {
        return {0, 0, 0, 0};
    }
    float proximity = (60.0f - distanceCm) / 50.0f;
    if (proximity > 1.0f) proximity = 1.0f;
    // Pleasant, comfortable acoustic tuning:
    // - Short, gentle pips (25ms - 45ms) instead of long ear-piercing screeches
    // - Relaxed, progressive intervals (850ms down to 140ms) giving ears breathing room
    // - Warm, pleasant musical frequency range (700 Hz - 1100 Hz) instead of shrill 2800 Hz
    return {
        static_cast<uint8_t>(100 + proximity * 155),
        static_cast<uint16_t>(25 + proximity * 20),
        static_cast<uint16_t>(850 - proximity * 710),
        static_cast<uint16_t>(700 + proximity * 400)
    };
}

inline GroundHazardFeedback feedbackForGround(float tofDistanceCm, float baselineCm = 115.0f) {
    // If ToF distance is invalid or out of range when angled downward (> baseline + 35 cm or <= 0)
    // it signifies the ground has dropped away (drop-off / down-stairs / pit)
    if (!isfinite(tofDistanceCm) || tofDistanceCm <= 0.0f || tofDistanceCm > (baselineCm + 35.0f)) {
        // Drop-off warning: Strong distinct pulsed vibration on Motor 2
        return { GroundHazardType::DROP_OFF, 255, 180, 120 };
    }

    // Step-up / curb up (significantly closer than baseline, e.g. < baseline - 35 cm)
    if (tofDistanceCm < (baselineCm - 35.0f) && tofDistanceCm > 15.0f) {
        // Step-up warning: Moderate steady buzz on Motor 2
        return { GroundHazardType::STEP_UP, 180, 400, 200 };
    }

    return { GroundHazardType::NONE, 0, 0, 0 };
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
