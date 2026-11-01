#include "TransportIntegration.h"

#include "Fixtures.h"

namespace AudioTest::TapTempo
{

void runTransportIntegration()
{
    std::cout << "\n-- transport integration --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (100.0);

    const double taps[] { 0.0, 0.50, 1.02, 1.50 };
    double estimate = 0.0;

    for (double seconds : taps)
        estimate = transport.tapAt (seconds);

    check (nearlyEqual (estimate, 120.0, 1.0),
           "Transport::tapAt() converges to 120 +/- 1 BPM");
    check (nearlyEqual (transport.getBpm(), 120.0, 1.0),
           "an internal transport adopts the tapped tempo");
    check (nearlyEqual (transport.getTapTempoBpm(), 120.0, 1.0),
           "the transport snapshot carries the tap estimate");
    check (nearlyEqual (transport.snapshot().tapTempoBpm, 120.0, 1.0),
           "Transport::snapshot().tapTempoBpm matches the estimate");

    transport.resetTapTempo();
    check (transport.getTapTempoBpm() == 0.0, "resetTapTempo() clears the estimate");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    transport.setBpm (100.0);
    transport.tapAt (10.0);
    const double externalEstimate = transport.tapAt (10.5);

    check (nearlyEqual (externalEstimate, 120.0, 1.0e-12),
           "tapping still estimates tempo while externally synced");
    check (transport.getBpm() == 100.0,
           "a successful tap does not override an external tempo");
}

}
