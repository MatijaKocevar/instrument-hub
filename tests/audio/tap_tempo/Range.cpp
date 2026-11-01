#include "Range.h"

#include "Fixtures.h"

namespace AudioTest::TapTempo
{

void runRange()
{
    std::cout << "\n-- tempo range --" << std::endl;

    struct Case
    {
        double interval;
        double expectedBpm;
    };

    const Case cases[]
    {
        { 0.200, AudioEngine::TapTempo::maxBpm },
        { 0.250, 240.0 },
        { 0.333333333333333, 180.0 },
        { 0.500, 120.0 },
        { 0.666666666666667, 90.0 },
        { 1.000, 60.0 },
        { 3.000, AudioEngine::TapTempo::minBpm },
    };

    for (const auto& testCase : cases)
    {
        AudioEngine::TapTempo tempo;
        tempo.tapAt (0.0);
        const double estimate = tempo.tapAt (testCase.interval);
        const double tolerance = testCase.expectedBpm * 1.0e-9;

        check (nearlyEqual (estimate, testCase.expectedBpm, tolerance),
               "an interval of " + std::to_string (testCase.interval)
                   + " s gives " + std::to_string (testCase.expectedBpm) + " BPM");
    }

    check (nearlyEqual (AudioEngine::TapTempo::intervalSecondsForBpm (120.0),
                        0.5, 1.0e-12),
           "intervalSecondsForBpm(120) is 0.5");
    check (nearlyEqual (AudioEngine::TapTempo::bpmForIntervalSeconds (0.5),
                        120.0, 1.0e-12),
           "bpmForIntervalSeconds(0.5) is 120");
    check (AudioEngine::TapTempo::intervalSecondsForBpm (0.0) == 0.0,
           "a non-positive tempo has no interval");
    check (AudioEngine::TapTempo::bpmForIntervalSeconds (0.0) == 0.0,
           "a non-positive interval has no tempo");
}

}
