#include "TempoChange.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runTempoChange()
{
    std::cout << "\n-- tempo change while running --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 96000, kBlockSize);   // 2 s at 120

    check (nearlyEqual (simulator.clockIn.getBpm(), 120.0, 0.05),
           "the first tempo is locked at 120 BPM");

    simulator.setTempo (90.0);
    simulator.run (fixture.transport, 96000, kBlockSize);   // 2 s at 90

    std::cout << "after 120 -> 90 BPM: estimate " << simulator.clockIn.getBpm()
              << ", transport " << fixture.transport.getBpm() << std::endl;

    check (simulator.clockIn.isLocked(),
           "the lock survives a tempo change");
    check (nearlyEqual (simulator.clockIn.getBpm(), 90.0, 0.5),
           "the estimate follows a 120 -> 90 BPM change within 2 s");
    check (nearlyEqual (fixture.transport.getBpm(), 90.0, 0.5),
           "the transport follows the new tempo");
}

} // namespace AudioTest::ClockIn
