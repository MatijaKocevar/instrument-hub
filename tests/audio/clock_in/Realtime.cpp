#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace AudioTest::ClockIn
{

void runRealtime()
{
    std::cout << "\n-- process() is allocation free --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 48000, kBlockSize);

    check (simulator.clockIn.isLocked(), "the clock is locked before the loop");

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 10000; ++block)
    {
        if (block % 4 == 0)
            simulator.clockIn.pushRealtime (0xF8, static_cast<double> (block) * 128.0 / kSampleRate);

        simulator.clockIn.process (fixture.transport, kBlockSize);
        fixture.transport.process (kBlockSize);
    }

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "allocations during 10000 push+process pairs: "
              << allocCount::total.load (std::memory_order_relaxed) << std::endl;

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "10000 push/process pairs allocate nothing");
}

}
