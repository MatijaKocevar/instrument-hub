#include "Jitter.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runJitter()
{
    std::cout << "\n-- timestamp jitter --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.setJitter (0.003, 0x5EED1234u);   // +/- 3 ms on a 20.8 ms tick
    simulator.start();

    simulator.run (fixture.transport, 144000, kBlockSize);   // 3 s

    std::cout << "jittered estimate: " << simulator.clockIn.getBpm()
              << " BPM, ticks " << simulator.clockIn.getTickCount() << std::endl;

    check (simulator.clockIn.isLocked(),
           "a jittered clock still locks");
    check (nearlyEqual (simulator.clockIn.getBpm(), 120.0, 1.0),
           "jitter of +/- 3 ms moves the estimate by less than 1 BPM");
    check (std::abs (simulator.clockIn.getTickCount() - 143) <= 1,
           "jitter does not lose or invent ticks");
    check (nearlyEqual (fixture.transport.getPpq(), 6.0, 2.0 * tick),
           "the jittered position stays within two ticks of 3 s of music");
}

} // namespace AudioTest::ClockIn
