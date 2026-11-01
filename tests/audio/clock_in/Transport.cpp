#include "Transport.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runTransport()
{
    std::cout << "\n-- start, stop and continue --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();

    simulator.run (fixture.transport, 48000, kBlockSize);   // 1 s

    const double heldPpq = fixture.transport.getPpq();
    // Holding is exact to within the block-quantised interpolation phase.
    const double holdTolerance = static_cast<double> (kBlockSize) / (kSampleRate * 60.0 / 120.0);

    check (simulator.clockIn.isPlaying() && fixture.transport.isPlaying(),
           "the transport is playing after one second");
    check (heldPpq > 1.9, "one second at 120 BPM is roughly 2 quarter notes");

    simulator.stop();
    simulator.run (fixture.transport, 2 * kBlockSize, kBlockSize);

    check (! simulator.clockIn.isPlaying(),
           "a stop message clears the playing state");
    check (! fixture.transport.isPlaying(),
           "a stop message stops the transport");
    check (simulator.clockIn.isLocked(),
           "a stopped master that still sends clock keeps the tempo lock");
    check (nearlyEqual (fixture.transport.getPpq(), heldPpq, holdTolerance),
           "the position is held while stopped");

    // The master keeps sending clock while stopped: the position must not move.
    simulator.run (fixture.transport, 48000, kBlockSize);
    check (nearlyEqual (fixture.transport.getPpq(), heldPpq, holdTolerance),
           "clock while stopped does not advance the position");
    check (nearlyEqual (simulator.clockIn.getBpm(), 120.0, 0.05),
           "tempo keeps tracking while stopped");

    simulator.resume();
    simulator.run (fixture.transport, 48000, kBlockSize);

    std::cout << "after continue: transport ppq " << fixture.transport.getPpq() << std::endl;

    check (fixture.transport.isPlaying(),
           "continue restarts the transport");
    check (nearlyEqual (fixture.transport.getPpq(), heldPpq + 2.0, 2.0 * tick),
           "continue resumes from the held position and advances 2 quarter notes");

    // Start rewinds to zero.
    simulator.stop();
    simulator.run (fixture.transport, kBlockSize, kBlockSize);
    simulator.start();
    simulator.run (fixture.transport, kBlockSize, kBlockSize);

    check (simulator.clockIn.getTickCount() <= 1,
           "start resets the tick count");
    check (fixture.transport.getPpq() < 2.0 * tick,
           "start resets the transport to the beginning");
}

} // namespace AudioTest::ClockIn
