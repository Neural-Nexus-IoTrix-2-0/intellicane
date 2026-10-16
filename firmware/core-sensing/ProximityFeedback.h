#pragma once

#include <stdint.h>
#include <math.h>

struct ProximityFeedback {
    uint8_t motorDuty;
    uint16_t motorPulseOnMs;
    uint16_t motorPulseOffMs;
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
    uint8_t motorDuty;      // PWM duty (0-255); ground feedback is not wired into the active loop
    uint16_t pulseOnMs;
    uint16_t pulseOffMs;
};

constexpr uint16_t PLEASANT_ALERT_TONE_HZ = 520; // 520 Hz: rich, low-frequency comfortable tone with high acoustic volume

inline ProximityFeedback feedbackForDistance(float distanceCm) {
    // No echo, invalid readings and the 60 cm boundary must not drive the motor.
    if (!isfinite(distanceCm) || distanceCm <= 0.0f || distanceCm >= 60.0f) {
        return {0, 0, 0, 0, 0, 0};
    }
    float proximity = (60.0f - distanceCm) / 50.0f;
    if (proximity > 1.0f) proximity = 1.0f;

    // Rich, widely spaced low-frequency cues at range; stronger and faster as the
    // obstacle approaches. Keep a minimum commanded duty so the motor starts.
    uint8_t motorDuty = static_cast<uint8_t>(135 + proximity * 120);
    uint16_t beepOnMs = static_cast<uint16_t>(80 + proximity * 40);
    uint16_t beepOffMs = static_cast<uint16_t>(1100 - proximity * 880);

    // Pulse the GPIO 2 motor only in the caution zone. It becomes continuous at 40 cm
    // so an approaching obstacle cannot disappear between pulses.
    uint16_t motorOnMs = 0;
    uint16_t motorOffMs = 0;
    if (distanceCm > 40.0f) {
        float caution = (60.0f - distanceCm) / 20.0f;
        motorOnMs = 150;
        motorOffMs = static_cast<uint16_t>(700 - caution * 350);
    }

    return {motorDuty, motorOnMs, motorOffMs, beepOnMs, beepOffMs,
            PLEASANT_ALERT_TONE_HZ};
}

inline GroundHazardFeedback feedbackForGround(float tofDistanceCm, float baselineCm = 115.0f) {
    // If ToF distance is invalid or out of range when angled downward (> baseline + 35 cm or <= 0)
    // it signifies the ground has dropped away (drop-off / down-stairs / pit)
    if (!isfinite(tofDistanceCm) || tofDistanceCm <= 0.0f || tofDistanceCm > (baselineCm + 35.0f)) {
        // Drop-off warning: Strong distinct pulsed vibration
        return { GroundHazardType::DROP_OFF, 255, 180, 120 };
    }

    // Step-up / curb up (significantly closer than baseline, e.g. < baseline - 35 cm)
    if (tofDistanceCm < (baselineCm - 35.0f) && tofDistanceCm > 15.0f) {
        // Step-up warning: Moderate pulsed vibration
        return { GroundHazardType::STEP_UP, 180, 400, 200 };
    }

    return { GroundHazardType::NONE, 0, 0, 0 };
}

inline uint8_t motorOutputDuty(uint8_t strength, bool activeLow) {
    return activeLow ? 255 - strength : strength;
}

class PulseEnvelope {
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
