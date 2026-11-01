#include "StartStop.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runStartStop()
{
    std::cout << "\n-- start, stop, continue --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);

    advance (transport, 48000, 128);
    check (transport.getPpq() == 0.0 && transport.getPositionSamples() == 0,
           "a stopped transport processes frames without moving");

    transport.start();
    advance (transport, 48000, 128);
    check (nearlyEqual (transport.getPpq(), 2.0, 1.0e-12),
           "1 s at 120 BPM advances 2 quarter notes");

    transport.stop();
    advance (transport, 96000, 128);
    check (nearlyEqual (transport.getPpq(), 2.0, 1.0e-12)
               && transport.getPositionSamples() == 48000,
           "stopping holds PPQ and sample position while frames continue");

    transport.start();
    advance (transport, 432000, 128);
    check (nearlyEqual (transport.getPpq(), 20.0, 1.0e-9),
           "continue reaches 20 PPQ after 10 s of playing time");
    check (transport.getPositionSamples() == 480000,
           "total playing frames are 480000");

    transport.setPlaying (true);
    check (transport.isPlaying(), "setPlaying(true) starts the transport");
    transport.setPlaying (false);
    check (! transport.isPlaying(), "setPlaying(false) stops the transport");
}

}
