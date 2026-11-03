#include "api/StateStore.h"

#include <utility>

namespace Api
{

void StateStore::setNumSlots (int newNumSlots)
{
    const int next = juce::jmax (1, newNumSlots);

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (next == numSlots.load (std::memory_order_relaxed))
            return;

        numSlots.store (next, std::memory_order_release);
        armedSlot = -1;
        families.clear();
        slotNames.assign (static_cast<std::size_t> (next), {});
        slotFamilies.assign (static_cast<std::size_t> (next), {});
        slotFamilyNames.assign (static_cast<std::size_t> (next), {});
        slotPatches.assign (static_cast<std::size_t> (next), {});
        slotPatchNames.assign (static_cast<std::size_t> (next), {});
        slotLatencySamples.assign (static_cast<std::size_t> (next), 0);
        slotLatencyMs.assign (static_cast<std::size_t> (next), 0.0);
        slotInstruments.assign (static_cast<std::size_t> (next), {});
        slotMixes.assign (static_cast<std::size_t> (next), {});
        slotEditorOpen.assign (static_cast<std::size_t> (next), false);

        for (int index = 0; index < next; ++index)
        {
            slotNames[static_cast<std::size_t> (index)] = "Slot "
                + std::to_string (index + 1);
            slotInstruments[static_cast<std::size_t> (index)].name =
                slotNames[static_cast<std::size_t> (index)];
        }
    }

    notifyChanged ("slots", -1);
}

void StateStore::setLayout (std::string newLayout)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);
        layout = std::move (newLayout);
    }

    notifyChanged ("layout", -1);
}

std::string StateStore::getLayout() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return layout;
}

}
