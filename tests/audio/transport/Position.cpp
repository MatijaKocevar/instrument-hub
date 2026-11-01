#include "Position.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runPosition()
{
    std::cout << "\n-- rewind and reposition --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    advance (transport, 480000, 128);

    transport.rewind();
    check (transport.getPpq() == 0.0 && transport.getPositionSamples() == 0,
           "rewind clears both positions");
    check (transport.isPlaying() && transport.getBpm() == 120.0,
           "rewind keeps playing state and tempo");

    transport.setPpq (8.0);
    check (transport.getPositionSamples() == 192000,
           "PPQ 8 at 120 BPM / 48 kHz maps to 192000 frames");

    transport.setPositionSamples (24000);
    check (nearlyEqual (transport.getPpq(), 1.0, 1.0e-12),
           "24000 frames map back to PPQ 1");

    transport.setPpq (-3.0);
    check (transport.getPpq() == 0.0 && transport.getPositionSamples() == 0,
           "a negative PPQ is clamped to zero");

    transport.setPositionSamples (-5);
    check (transport.getPositionSamples() == 0,
           "a negative sample position is clamped to zero");
}

}
