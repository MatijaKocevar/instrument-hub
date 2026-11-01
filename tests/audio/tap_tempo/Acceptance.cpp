#include "Acceptance.h"

#include "Fixtures.h"

namespace AudioTest::TapTempo
{

void runAcceptance()
{
    std::cout << "\n-- acceptance: 4 taps at 500 ms +/- 20 ms -> 120 +/- 1 BPM --"
              << std::endl;

    const double taps[] { 0.0, 0.50, 1.02, 1.50 };
    const double estimate = fourTaps (taps);

    std::cout << "     estimate after 4 taps: " << estimate
              << " BPM (expected 120 +/- 1)" << std::endl;

    check (nearlyEqual (estimate, 120.0, 1.0),
           "4 taps at 500 ms +/- 20 ms converge to 120 +/- 1 BPM");

    const double earlyMiddle[] { 0.0, 0.48, 0.98, 1.50 };
    const double lateMiddle[]  { 0.0, 0.52, 1.00, 1.50 };

    std::cout << "     early-middle pattern: " << fourTaps (earlyMiddle)
              << " BPM" << std::endl;
    std::cout << "     late-middle pattern:  " << fourTaps (lateMiddle)
              << " BPM" << std::endl;

    check (nearlyEqual (fourTaps (earlyMiddle), 120.0, 1.0),
           "taps jittered early in the middle still converge to 120 +/- 1 BPM");
    check (nearlyEqual (fourTaps (lateMiddle), 120.0, 1.0),
           "taps jittered late in the middle still converge to 120 +/- 1 BPM");

    AudioEngine::TapTempo tempo;
    check (tempo.tapAt (0.0) == 0.0, "one tap gives no estimate");
    check (tempo.getTapCount() == 1, "one tap is counted");
    check (! tempo.hasEstimate(), "hasEstimate() is false after one tap");
    check (nearlyEqual (tempo.tapAt (0.5), 120.0, 1.0e-12),
           "two taps 500 ms apart give 120 BPM");
    check (tempo.getTapCount() == 2, "two taps are counted");
    check (tempo.getIntervalCount() == 1, "one interval is in the window");
    check (tempo.hasEstimate(), "hasEstimate() is true after two taps");
}

}
