#include "Median.h"

#include "Fixtures.h"

namespace AudioTest::TapTempo
{

void runMedian()
{
    std::cout << "\n-- median-of-last-N --" << std::endl;

    AudioEngine::TapTempo tempo;

    for (int index = 0; index <= 8; ++index)
        tempo.tapAt (0.5 * index);

    check (nearlyEqual (tempo.getBpm(), 120.0, 1.0e-12),
           "eight steady 500 ms intervals give exactly 120 BPM");
    check (tempo.getIntervalCount() == AudioEngine::TapTempo::defaultHistorySize,
           "the interval window fills to N");

    check (nearlyEqual (tempo.tapAt (4.52), 120.0, 1.0e-12),
           "one late tap does not move a settled 120 BPM tempo");
    check (nearlyEqual (tempo.tapAt (5.00), 120.0, 1.0e-12),
           "a late tap followed by an early one still reads 120 BPM");

    AudioEngine::TapTempo shortMemory (3);

    shortMemory.tapAt (0.0);
    shortMemory.tapAt (0.5);
    shortMemory.tapAt (1.0);
    shortMemory.tapAt (1.5);
    check (nearlyEqual (shortMemory.getBpm(), 120.0, 1.0e-12),
           "a window of 3 steady 500 ms intervals gives 120 BPM");

    shortMemory.tapAt (1.9);
    check (nearlyEqual (shortMemory.getBpm(), 120.0, 1.0e-12),
           "the first 400 ms interval is outvoted 2:1");

    shortMemory.tapAt (2.3);
    check (nearlyEqual (shortMemory.getBpm(), 150.0, 1.0e-12),
           "a 3-interval window follows a new 400 ms tempo after two taps");

    shortMemory.tapAt (2.7);
    check (nearlyEqual (shortMemory.getBpm(), 150.0, 1.0e-12),
           "the window has fully forgotten the old tempo");

    check (shortMemory.getHistorySize() == 3, "the configured history size is reported");
    check (AudioEngine::TapTempo (0).getHistorySize() == 1,
           "a zero history size is clamped to one interval");
    check (AudioEngine::TapTempo (1000).getHistorySize()
               == AudioEngine::TapTempo::maximumHistorySize,
           "an oversized history is clamped to the fixed storage");
}

}
