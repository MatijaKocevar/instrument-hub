#include "Tempos.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runTempos()
{
    std::cout << "\n-- other tempos --" << std::endl;

    for (const double bpm : { 60.0, 90.0, 180.0, 240.0 })
    {
        Fixture fixture;
        Simulator simulator (kSampleRate, bpm);
        simulator.start();

        simulator.run (fixture.transport, 120000, kBlockSize);

        const double expectedTicks = 2.5 * bpm / 60.0 * 24.0;
        const double expectedPpq = 2.5 * bpm / 60.0;

        std::cout << bpm << " BPM -> estimate " << simulator.clockIn.getBpm()
                  << ", ticks " << simulator.clockIn.getTickCount()
                  << ", ppq " << fixture.transport.getPpq() << std::endl;

        check (simulator.clockIn.isLocked(),
               std::to_string (static_cast<int> (bpm)) + " BPM locks");
        check (nearlyEqual (simulator.clockIn.getBpm(), bpm, 0.05),
               std::to_string (static_cast<int> (bpm)) + " BPM is estimated correctly");
        check (nearlyEqual (static_cast<double> (simulator.clockIn.getTickCount()),
                            expectedTicks, 1.5),
               std::to_string (static_cast<int> (bpm)) + " BPM advances one tick per tick");
        check (nearlyEqual (fixture.transport.getPpq(), expectedPpq, tick),
               std::to_string (static_cast<int> (bpm)) + " BPM position matches elapsed time");
    }
}

}
