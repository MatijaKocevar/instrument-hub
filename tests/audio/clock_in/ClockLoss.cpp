#include "ClockLoss.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runClockLoss()
{
    std::cout << "\n-- clock loss without a stop message --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 48000, kBlockSize);

    const double heldPpq = fixture.transport.getPpq();
    const double lastClockTime = static_cast<double> (simulator.frameIndex()) / kSampleRate;

    check (fixture.transport.isPlaying(), "the transport is playing before the loss");

    simulator.disappear();
    simulator.run (fixture.transport, 36000, kBlockSize);

    std::cout << "after " << (simulator.frameIndex() / kSampleRate - lastClockTime)
              << " s without clock: locked " << simulator.clockIn.isLocked()
              << ", playing " << simulator.clockIn.isPlaying() << std::endl;

    check (! simulator.clockIn.isClockPresent(),
           "silence is detected as a lost clock");
    check (! simulator.clockIn.isLocked(),
           "a lost clock drops the tempo lock");
    check (! fixture.transport.isPlaying(),
           "a lost clock stops the transport");
    check (! fixture.transport.isExternalClockLocked(),
           "the transport reports the external clock as lost");

    const double stoppedPpq = fixture.transport.getPpq();
    check (stoppedPpq >= heldPpq && stoppedPpq <= heldPpq + 1.1,
           "the transport rolled only through the detection window");

    simulator.run (fixture.transport, 48000, kBlockSize);

    check (fixture.transport.getPpq() == stoppedPpq,
           "after the loss is detected the position no longer advances");

    simulator.resume();
    simulator.run (fixture.transport, 36000, kBlockSize);

    check (simulator.clockIn.isLocked(),
           "the clock relocks after it returns");
    check (fixture.transport.isPlaying(),
           "the transport resumes after the clock returns");

    Fixture slow;
    Simulator slowSimulator (kSampleRate, 20.0);
    slowSimulator.start();
    slowSimulator.run (slow.transport, 192000, kBlockSize);

    check (slowSimulator.clockIn.isClockPresent(),
           "a 20 BPM clock never trips the loss timeout");
}

}
