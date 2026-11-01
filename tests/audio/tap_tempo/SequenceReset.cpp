#include "SequenceReset.h"

#include "Fixtures.h"

namespace AudioTest::TapTempo
{

void runSequenceReset()
{
    std::cout << "\n-- sequence resets --" << std::endl;

    AudioEngine::TapTempo tempo;

    tempo.tapAt (0.0);
    tempo.tapAt (0.5);
    check (tempo.getBpm() > 0.0, "a 120 BPM sequence has an estimate");

    // 3.5 s exceeds 60 / 20 = 3 s.
    check (tempo.tapAt (4.0) == 0.0, "a gap longer than 3 s starts a new sequence");
    check (tempo.getTapCount() == 1, "the new sequence has one tap");
    check (tempo.getIntervalCount() == 0, "the new sequence has no intervals");
    check (tempo.getLastIntervalSeconds() == 0.0, "the last interval is cleared");
    check (nearlyEqual (tempo.tapAt (4.5), 120.0, 1.0e-12),
           "tapping resumes after the gap");

    check (tempo.tapAt (4.5) == 0.0,
           "a repeated timestamp starts a new sequence instead of dividing by zero");
    check (tempo.tapAt (4.0) == 0.0,
           "a backwards timestamp starts a new sequence");
    check (nearlyEqual (tempo.tapAt (4.5), 120.0, 1.0e-12),
           "the backwards tap left a clean one-tap sequence");

    // Intervals shorter than 200 ms cannot be a supported tempo.
    AudioEngine::TapTempo fast;
    fast.tapAt (10.0);
    check (fast.tapAt (10.1) == 0.0,
           "an interval below 200 ms starts a new sequence");

    // Non-finite input leaves the state alone.
    AudioEngine::TapTempo robust;
    robust.tapAt (20.0);
    robust.tapAt (20.5);
    const int tapsBefore = robust.getTapCount();

    check (nearlyEqual (robust.tapAt (std::numeric_limits<double>::infinity()),
                        120.0, 1.0e-12),
           "an infinite timestamp returns the current estimate");
    check (nearlyEqual (robust.tapAt (std::numeric_limits<double>::quiet_NaN()),
                        120.0, 1.0e-12),
           "a NaN timestamp returns the current estimate");
    check (robust.getTapCount() == tapsBefore,
           "non-finite timestamps do not record a tap");
}

} // namespace AudioTest::TapTempo
