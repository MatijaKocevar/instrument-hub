#include "audio/FxRack.h"

#include <algorithm>

namespace AudioEngine
{

    void InsertEffect::refreshLatency () noexcept
    {
        const bool audible = !bypassed.load (std::memory_order_relaxed) &&
                             wet.load (std::memory_order_relaxed) > 0.0f;

        const int latency =
            (processor != nullptr && audible) ? juce::jmax (0, processor->getLatencySamples ()) : 0;

        cachedLatency.store (latency, std::memory_order_relaxed);

        setLatencySamples (latency);
    }

    int FxRack::latencySamples () const noexcept
    {
        int total = 0;

        for (const auto& insert : inserts)
            if (insert.effect != nullptr)
                total += juce::jmax (0, insert.effect->getLatencySamples ());

        return total;
    }

}
