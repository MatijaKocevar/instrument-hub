#include "Tempo.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runTempo()
{
    std::cout << "\n-- tempo changes and sample rates --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (60.0);
    transport.start();

    advance (transport, 240000, 128);
    check (nearlyEqual (transport.getPpq(), 5.0, 1.0e-12),
           "5 s at 60 BPM advances 5 quarter notes");

    transport.setBpm (120.0);
    advance (transport, 240000, 128);
    check (nearlyEqual (transport.getPpq(), 15.0, 1.0e-12),
           "a tempo change applies at the next block and keeps the position");
    check (nearlyEqual (transport.getSamplesPerQuarterNote(), 24000.0, 1.0e-9),
           "the tempo change updates samples per quarter note");

    transport.setBpm (1000.0);
    check (transport.getBpm() == AudioEngine::Transport::maxBpm,
           "tempo is clamped to the 300 BPM maximum");
    transport.setBpm (1.0);
    check (transport.getBpm() == AudioEngine::Transport::minBpm,
           "tempo is clamped to the 20 BPM minimum");
    transport.setBpm (std::numeric_limits<double>::quiet_NaN());
    check (transport.getBpm() == AudioEngine::Transport::minBpm,
           "a non-finite tempo is ignored");

    AudioEngine::Transport fast;
    fast.prepare (96000.0, 256);
    fast.setBpm (120.0);
    fast.start();
    advance (fast, 480000, 256);

    check (nearlyEqual (fast.getPpq(), 10.0, 1.0e-12),
           "480000 frames at 96 kHz are 5 s, i.e. 10 quarter notes at 120 BPM");
    check (nearlyEqual (fast.getSamplesPerQuarterNote(), 48000.0, 1.0e-9),
           "one quarter note is 48000 frames at 120 BPM / 96 kHz");
}

}
