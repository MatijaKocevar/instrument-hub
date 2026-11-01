#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace AudioTest::Transport
{

void runRealtime()
{
    std::cout << "\n-- process() is allocation free --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 10000; ++block)
        transport.process (128);

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "allocations during 10000 process() calls: "
              << allocCount::total.load (std::memory_order_relaxed) << std::endl;

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "10000 process() calls allocate nothing");
    check (nearlyEqual (transport.getPpq(), 10000.0 * 128.0 / 24000.0, 1.0e-9),
           "the rendered position is exact after 10000 blocks");
}

}
