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
        assert(f.motorDuty == 0 && f.beepOnMs == 0 && f.toneHz == 0);
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
        previous = f;
    }
    assert(feedbackForDistance(10).motorDuty == 255);
    assert(feedbackForDistance(1).motorDuty == 255);
    assert(motorOutputDuty(0, false) == 0);
    assert(motorOutputDuty(0, true) == 255);
    assert(motorOutputDuty(255, true) == 0);

    BeepEnvelope beep;
    assert(beep.update(1000, 60, 740, 1));
    assert(!beep.update(1060, 60, 740, 1));
    assert(beep.update(1800, 60, 740, 1));
    assert(!beep.update(1801, 0, 0, 0));
    assert(beep.update(1802, 60, 740, 1)); // Immediate re-entry.
    assert(beep.update(1803, 200, 200, 2)); // Fall mode transition.
    assert(!beep.update(2003, 200, 200, 2));
    assert(!beep.update(2004, 0, 0, 0));
    assert(beep.update(UINT32_MAX - 20, 60, 740, 1));
    assert(beep.update(10, 60, 740, 1)); // millis rollover.
    assert(!beep.update(40, 60, 740, 1));
    puts("PASS: boundaries, invalid ranges, monotonic feedback, polarity, beep transitions and rollover");
}
