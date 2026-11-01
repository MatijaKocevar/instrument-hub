#include "InternalSync.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runInternalSync()
{
    std::cout << "\n-- internal sync is untouched --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (kSampleRate, kBlockSize);
    transport.setBpm (100.0);

    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (transport, 96000, kBlockSize);

    check (simulator.clockIn.isLocked(),
           "the follower still estimates the clock");
    check (! transport.isPlaying(),
           "clock updates do not start a transport in internal sync");
    check (transport.getBpm() == 100.0,
           "clock updates do not change an internal tempo");
    check (transport.getPpq() == 0.0,
           "clock updates do not move an internal position");
}

} // namespace AudioTest::ClockIn
