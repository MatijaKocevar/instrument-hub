#include "TapTempo.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runTapTempo()
{
    std::cout << "\n-- tap tempo --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);

    check (transport.tapAt (0.0) == 0.0,
           "one tap is not enough for a tempo estimate");
    check (nearlyEqual (transport.tapAt (0.5), 120.0, 1.0e-12),
           "two taps 500 ms apart give 120 BPM");

    transport.resetTapTempo();
    transport.tapAt (0.0);
    transport.tapAt (0.5);
    transport.tapAt (1.0);
    transport.tapAt (1.5);

    check (nearlyEqual (transport.getTapTempoBpm(), 120.0, 1.0e-12),
           "four even taps give 120 BPM");
    check (transport.getBpm() == 120.0,
           "tap tempo writes the internal tempo");

    transport.resetTapTempo();
    transport.tapAt (0.0);
    transport.tapAt (0.48);
    transport.tapAt (1.00);
    transport.tapAt (1.48);
    const double jittered = transport.tapAt (2.00);

    check (nearlyEqual (jittered, 120.0, 1.0),
           "taps spaced 500 ms +/- 20 ms converge to 120 +/- 1 BPM");

    transport.resetTapTempo();
    transport.tapAt (0.0);
    transport.tapAt (0.5);
    const double gapReset = transport.tapAt (4.0);   // 3.5 s gap, outside range
    check (gapReset == 0.0,
           "a gap outside the supported tempo range starts a new sequence");
    check (nearlyEqual (transport.tapAt (4.5), 120.0, 1.0e-12),
           "tapping resumes after the gap");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    transport.setBpm (100.0);
    transport.resetTapTempo();
    transport.tapAt (10.0);
    const double externalTap = transport.tapAt (10.5);

    check (nearlyEqual (externalTap, 120.0, 1.0e-12),
           "tapping still estimates tempo while externally synced");
    check (transport.getBpm() == 100.0,
           "tap tempo does not override an external tempo");
}

} // namespace AudioTest::Transport
