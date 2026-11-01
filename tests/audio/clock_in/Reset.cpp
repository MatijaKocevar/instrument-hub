#include "Reset.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runReset()
{
    std::cout << "\n-- reset and snapshot --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 96000, kBlockSize);

    auto snapshot = simulator.clockIn.snapshot();

    check (snapshot.locked && snapshot.playing && snapshot.clockPresent,
           "the snapshot reports a locked, playing, present clock");
    check (nearlyEqual (snapshot.bpm, 120.0, 0.01)
               && nearlyEqual (snapshot.ppq, 4.0, tick),
           "the snapshot carries tempo and position");

    simulator.clockIn.reset();

    check (! simulator.clockIn.isLocked()
               && ! simulator.clockIn.isPlaying()
               && ! simulator.clockIn.isClockPresent()
               && simulator.clockIn.getBpm() == 0.0
               && simulator.clockIn.getPpq() == 0.0
               && simulator.clockIn.getTickCount() == 0,
           "reset clears every published field");
    check (simulator.clockIn.getPendingEventCount() == 0,
           "reset clears the queue");
}

} // namespace AudioTest::ClockIn
