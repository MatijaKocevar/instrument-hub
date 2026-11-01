#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace AudioTest::ClockOut
{

void runRealtime()
{
    std::cout << "\n-- process() is allocation free --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    AudioEngine::ClockOut clock;
    clock.process (transport, 128);
    clock.clearQueue();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 10000; ++block)
    {
        clock.process (transport, 128);
        transport.process (128);

        allocCount::enabled.store (false, std::memory_order_relaxed);
        clock.clearQueue();
        allocCount::enabled.store (true, std::memory_order_relaxed);
    }

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "allocations during 10000 process() calls: "
              << allocCount::total.load (std::memory_order_relaxed) << std::endl;

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "10000 process() calls allocate nothing");
    check (clock.getDroppedEventCount() == 0,
           "clearQueue between blocks loses no events");
}

}
