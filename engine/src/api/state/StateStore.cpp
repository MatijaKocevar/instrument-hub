#include "api/StateStore.h"

#include <chrono>
#include <utility>

namespace Api
{

StateStore::StateStore (int numSlotsToUse)
    : numSlots (juce::jmax (1, numSlotsToUse)),
      startedAt (std::chrono::steady_clock::now())
{
    slotNames.reserve (static_cast<std::size_t> (numSlots));
    slotFamilies.assign (static_cast<std::size_t> (numSlots), {});
    slotFamilyNames.assign (static_cast<std::size_t> (numSlots), {});
    slotPatches.assign (static_cast<std::size_t> (numSlots), {});
    slotPatchNames.assign (static_cast<std::size_t> (numSlots), {});
    slotLatencySamples.assign (static_cast<std::size_t> (numSlots), 0);
    slotLatencyMs.assign (static_cast<std::size_t> (numSlots), 0.0);
    slotInstruments.assign (static_cast<std::size_t> (numSlots), {});
    slotMixes.assign (static_cast<std::size_t> (numSlots), {});
    slotEditorOpen.assign (static_cast<std::size_t> (numSlots), false);

    for (int index = 0; index < numSlots; ++index)
    {
        slotNames.push_back ("Slot " + std::to_string (index + 1));
        slotInstruments[static_cast<std::size_t> (index)].name = slotNames.back();
    }
}

int StateStore::getArmedSlot() const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);
    return armedSlot;
}

bool StateStore::setArmedSlot (int slot)
{
    ArmHandler handler;
    ChangeHandler changed;
    int appliedSlot = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot >= numSlots)
            return false;

        appliedSlot = slot < 0 ? -1 : slot;
        armedSlot = appliedSlot;
        handler = armHandler;
        changed = changeHandler;
    }

    if (handler != nullptr)
        handler (appliedSlot);

    if (changed != nullptr)
        changed ("arm", appliedSlot);

    return true;
}

void StateStore::setArmHandler (ArmHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    armHandler = std::move (handler);
}


void StateStore::setSlotRuntimeProvider (SlotRuntimeProvider provider)
{
    const std::lock_guard<std::mutex> lock (mutex);
    slotRuntimeProvider = std::move (provider);
}

void StateStore::setChangeHandler (ChangeHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    changeHandler = std::move (handler);
}

int StateStore::getNumSlots() const noexcept
{
    return numSlots;
}

void StateStore::setSlotName (int slot, std::string name)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
            return;

        slotNames[static_cast<std::size_t> (slot)] = std::move (name);
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", slot);
}

void StateStore::setSlotIdentity (int slot,
                                  std::string family,
                                  std::string familyName,
                                  std::string patch,
                                  std::string patchName)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
            return;

        const auto index = static_cast<std::size_t> (slot);
        slotFamilies[index] = std::move (family);
        slotFamilyNames[index] = std::move (familyName);
        slotPatches[index] = std::move (patch);
        slotPatchNames[index] = std::move (patchName);
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", slot);
}

void StateStore::setFamilyTree (std::vector<FamilyState> tree)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        families = std::move (tree);
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", -1);
}

std::vector<FamilyState> StateStore::getFamilyTree() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return families;
}


void StateStore::setSlotLatency (int slot, int samples, double sampleRate)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
            return;

        const int clampedSamples = juce::jmax (0, samples);
        const auto index = static_cast<std::size_t> (slot);

        slotLatencySamples[index] = clampedSamples;
        slotLatencyMs[index] = sampleRate > 0.0
                                   ? static_cast<double> (clampedSamples) * 1000.0 / sampleRate
                                   : 0.0;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", slot);
}

double StateStore::getUptimeMs() const noexcept
{
    const auto elapsed = std::chrono::steady_clock::now() - startedAt;
    return std::chrono::duration<double, std::milli> (elapsed).count();
}
} // namespace Api
