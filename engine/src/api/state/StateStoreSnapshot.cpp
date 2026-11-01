#include "api/StateStore.h"

#include <utility>

namespace Api
{

StateSnapshot StateStore::snapshot() const
{
    StateSnapshot result;
    TransportProvider transportReader;
    SlotRuntimeProvider slotReader;
    std::vector<PatchInstrumentState> instruments;
    std::vector<PatchMixState> mixes;
    std::vector<bool> editors;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        result.armedSlot = armedSlot;
        result.settings = settings;
        result.transport = transport;
        result.slots.reserve (static_cast<std::size_t> (numSlots));

        for (int index = 0; index < numSlots; ++index)
        {
            const auto position = static_cast<std::size_t> (index);

            SlotSnapshot slot;
            slot.index = index;
            slot.name = slotNames[position];
            slot.armed = index == armedSlot;
            slot.family = slotFamilies[position];
            slot.familyName = slotFamilyNames[position];
            slot.patch = slotPatches[position];
            slot.patchName = slotPatchNames[position];
            slot.latencySamples = slotLatencySamples[position];
            slot.latencyMs = slotLatencyMs[position];
            result.slots.push_back (std::move (slot));
        }

        result.families = families;
        instruments = slotInstruments;
        mixes = slotMixes;
        editors = slotEditorOpen;

        transportReader = transportProvider;
        slotReader = slotRuntimeProvider;
    }

    if (transportReader != nullptr)
    {
        try
        {
            result.transport = transportReader();
        }
        catch (...)
        {
        }
    }

    if (slotReader != nullptr)
    {
        for (auto& slot : result.slots)
        {
            try
            {
                const auto runtime = slotReader (slot.index);
                slot.armed = runtime.armed;
                slot.gain = runtime.gain;
                slot.state = runtime.state;
                slot.warmState = runtime.warmState;
                slot.warm = runtime.warm;
                slot.editorOpen = runtime.editorOpen;
            }
            catch (...)
            {
            }
        }
    }

    for (auto& family : result.families)
    {
        for (auto& patch : family.patches)
        {
            const int slot = patch.slot;

            if (slot < 0 || slot >= static_cast<int> (mixes.size())
                || slot >= static_cast<int> (result.slots.size()))
                continue;

            const auto index = static_cast<std::size_t> (slot);
            const auto& mix = mixes[index];
            const auto& live = result.slots[index];

            patch.instrument = instruments[index];
            patch.volumeDb = mix.volumeDb;
            patch.pan = mix.pan;
            patch.mute = mix.mute;
            patch.solo = mix.solo;
            patch.colorIndex = mix.colorIndex;

            if (patch.instrument.name.empty())
                patch.instrument.name = patch.name;

            patch.warmState = live.warmState;
            patch.warm = live.warm;
            patch.editorOpen = slotReader != nullptr
                                   ? live.editorOpen
                                   : (index < editors.size() && editors[index]);
            patch.latencySamples = live.latencySamples;
            patch.latencyMs = live.latencyMs;
        }
    }

    result.uptimeMs = getUptimeMs();
    return result;
}

}
