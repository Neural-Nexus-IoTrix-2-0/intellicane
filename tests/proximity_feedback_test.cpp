#include "../firmware/core-sensing/ProximityFeedback.h"
#include <assert.h>
#include <initializer_list>
#include <limits>
#include <stdio.h>

int main() {
    for (float d : {60.0f, 60.01f, 120.0f, 400.0f, 0.0f, -1.0f,
                   std::numeric_limits<float>::quiet_NaN(),
                   std::numeric_limits<float>::infinity()}) {
        auto f = feedbackForDistance(d);
        assert(f.motorDuty == 0);
        assert(f.motorPulseOnMs == 0 && f.motorPulseOffMs == 0);
        assert(f.beepOnMs == 0 && f.toneHz == 0);
    }
    auto previous = feedbackForDistance(59.99f);
    assert(previous.motorDuty > 0);
    for (int mm = 599; mm >= 1; --mm) {
        auto f = feedbackForDistance(mm / 10.0f);
        assert(f.motorDuty >= previous.motorDuty);
        assert(f.beepOnMs >= previous.beepOnMs);
        assert(f.beepOffMs <= previous.beepOffMs);
        assert(f.toneHz >= previous.toneHz);
        assert(f.beepOnMs + f.beepOffMs <= previous.beepOnMs + previous.beepOffMs);
        // The sole motor must provide a cue throughout the full alert range.
        assert(f.motorDuty > 0);
        if (mm <= 400) {
            assert(f.motorPulseOnMs == 0 && f.motorPulseOffMs == 0);
        } else {
            assert(f.motorPulseOnMs == 150 && f.motorPulseOffMs > 0);
        }
        previous = f;
    }
    auto caution = feedbackForDistance(50.0f);
    assert(caution.motorPulseOnMs == 150 && caution.motorPulseOffMs > 0);
    auto warning = feedbackForDistance(40.0f);
    assert(warning.motorDuty > 0 && warning.motorPulseOnMs == 0);
    assert(feedbackForDistance(25.0f).motorDuty >= warning.motorDuty);
    auto close = feedbackForDistance(20.0f);
    assert(close.motorDuty >= warning.motorDuty);
    assert(close.motorPulseOnMs == 0 && close.motorPulseOffMs == 0);
    assert(feedbackForDistance(10).motorDuty == 255);
    assert(feedbackForDistance(10).beepOffMs >= 200); // No near-continuous tone.
    assert(feedbackForDistance(1).motorDuty == 255);
    assert(motorOutputDuty(0, false) == 0);
    assert(motorOutputDuty(0, true) == 255);
    assert(motorOutputDuty(255, true) == 0);

    PulseEnvelope beep;
    assert(beep.update(1000, 60, 740, 1));
    assert(!beep.update(1060, 60, 740, 1));
    assert(beep.update(1800, 60, 740, 1));
    assert(!beep.update(1801, 0, 0, 0));
    assert(beep.update(1802, 60, 740, 1)); // Immediate re-entry.
    assert(beep.update(1803, 200, 200, 2)); // Fall mode transition.
    assert(!beep.update(2003, 200, 200, 2));
    assert(!beep.update(2004, 0, 0, 0));
    PulseEnvelope haptic;
    assert(haptic.update(1000, 150, 700, 1));
    assert(!haptic.update(1150, 150, 700, 1));
    assert(haptic.update(1850, 150, 700, 1));
    assert(!haptic.update(1851, 0, 0, 0));
    // The caution pulse must also remain correct across millis() rollover.
    PulseEnvelope rollover;
    assert(rollover.update(UINT32_MAX - 99, 150, 700, 1));
    assert(rollover.update(49, 150, 700, 1));
    assert(!rollover.update(50, 150, 700, 1));
    assert(rollover.update(750, 150, 700, 1));
    // Ground hazard (ToF laser) tests
    auto normalGround = feedbackForGround(115.0f);
    assert(normalGround.type == GroundHazardType::NONE && normalGround.motorDuty == 0);

    auto dropOff = feedbackForGround(160.0f); // Ground drops away (stairs down / hole)
    assert(dropOff.type == GroundHazardType::DROP_OFF && dropOff.motorDuty == 255);

    auto outOfRangeDrop = feedbackForGround(std::numeric_limits<float>::quiet_NaN());
    assert(outOfRangeDrop.type == GroundHazardType::DROP_OFF && outOfRangeDrop.motorDuty == 255);

    auto stepUp = feedbackForGround(65.0f); // Curb / step up
    assert(stepUp.type == GroundHazardType::STEP_UP && stepUp.motorDuty == 180);

    puts("PASS: range boundaries, single-motor cues, pulse transitions, rollover, and ground hazards");
}
