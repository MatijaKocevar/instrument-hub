#include "Resolve.h"

#include "../TestSupport.h"

#include <string>

namespace KitTest
{

using SamplerTest::check;

void runResolveChecks (ihub::sampler::LoadedKit& kit)
{
    allocCount::total.store (0);
    allocCount::enabled.store (true);

    volatile float sink = 0.0f;

    for (int i = 0; i < 20000; ++i)
    {
        const auto hit = kit.resolve (36, (i % 127) + 1);

        if (hit.isValid())
            sink += hit.gain + hit.pan + (float) hit.playbackRatio;
    }

    allocCount::enabled.store (false);

    check (allocCount::total.load() == 0,
           "resolve() allocates nothing (" + std::to_string (allocCount::total.load())
               + " allocations)");
    (void) sink;
}

}
