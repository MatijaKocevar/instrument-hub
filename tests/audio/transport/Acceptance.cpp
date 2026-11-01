#include "Acceptance.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runAcceptance()
{
    std::cout << "\n-- acceptance: 10 s at 120 BPM is 20.0 PPQ --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 512);
    transport.setBpm (120.0);
    transport.start();

    advance (transport, 480000, 128);

    const double ppq = transport.getPpq();

    std::cout << "PPQ after 480000 frames: " << ppq << " (expected 20.0 +/- "
              << acceptanceTolerancePpq << ")" << std::endl;

    check (nearlyEqual (ppq, 20.0, acceptanceTolerancePpq),
           "10 s at 120 BPM / 48 kHz gives 20.0 PPQ within one tick");
    check (transport.getPositionSamples() == 480000,
           "position is exactly the 480000 frames that were processed");
    check (nearlyEqual (transport.getSamplesPerQuarterNote(), 24000.0, 1.0e-9),
           "one quarter note is 24000 frames at 120 BPM / 48 kHz");

    // A render with a different block size must give the same answer: the
    // position depends on frames, not on how they were grouped.
    AudioEngine::Transport blocked;
    blocked.prepare (48000.0, 512);
    blocked.setBpm (120.0);
    blocked.start();

    advance (blocked, 480000, 512);

    check (nearlyEqual (blocked.getPpq(), ppq, 1.0e-9),
           "512-frame blocks land on the same PPQ as 128-frame blocks");
}

} // namespace AudioTest::Transport
