#include "Reset.h"

#include "Fixtures.h"

namespace AudioTest::TapTempo
{

void runReset()
{
    std::cout << "\n-- reset --" << std::endl;

    AudioEngine::TapTempo tempo;
    tempo.tapAt (0.0);
    tempo.tapAt (0.5);
    tempo.reset();

    check (tempo.getBpm() == 0.0, "reset clears the estimate");
    check (! tempo.hasEstimate(), "reset clears hasEstimate()");
    check (tempo.getTapCount() == 0, "reset clears the tap count");
    check (tempo.getIntervalCount() == 0, "reset clears the interval window");
    check (tempo.getLastIntervalSeconds() == 0.0, "reset clears the last interval");

    check (nearlyEqual (tempo.tapAt (10.0 + 0.5), 0.0, 1.0e-12),
           "the tap after reset starts a new sequence");

    // The wall-clock overload is a thin wrapper around tapAt().
    AudioEngine::TapTempo wallClock;
    check (wallClock.tap() == 0.0, "tap() needs two calls for an estimate");
    check (wallClock.getTapCount() == 1, "tap() records the first tap");
}

} // namespace AudioTest::TapTempo
