#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace AudioTest::TapTempo
{

void runRealtime()
{
    std::cout << "\n-- allocation-free tapping --" << std::endl;

    AudioEngine::TapTempo tempo;

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int index = 0; index < 1000; ++index)
        tempo.tapAt (0.5 * index);

    allocCount::enabled.store (false, std::memory_order_relaxed);

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "1000 taps allocate nothing");
}

}
