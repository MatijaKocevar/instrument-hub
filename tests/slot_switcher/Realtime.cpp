#include "Realtime.h"
#include "AllocationCount.h"
#include "Fixtures.h"

#include "audio/SlotSwitcher.h"

#include <iostream>

namespace SlotSwitcherTest
{

void runNoAllocations()
{
    std::cout << "\n-- audio-thread allocation check --" << std::endl;

    SineSource sourceA (kAmplitude, kFrequency, 0.0);
    SineSource sourceB (kAmplitude, kFrequency, 1.0);

    AudioEngine::Slot slotA (0);
    AudioEngine::Slot slotB (1);
    slotA.setSource (&sourceA);
    slotB.setSource (&sourceB);

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slotA, &slotB });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::MidiBuffer midi;

    switcher.armSlot (0);

    for (int block = 0; block < kPreRollBlocks; ++block)
        switcher.process (kBlockSize, midi);

    allocCount::enabled.store (true, std::memory_order_relaxed);
    const long before = allocCount::total.load (std::memory_order_relaxed);

    for (int block = 0; block < 64; ++block)
    {
        if (block == 16)
            switcher.armSlot (1);

        if (block == 48)
            switcher.armSlot (0);

        switcher.process (kBlockSize, midi);
    }

    allocCount::enabled.store (false, std::memory_order_relaxed);
    const long after = allocCount::total.load (std::memory_order_relaxed);

    check (after == before, "process() allocates nothing across three switches");
}

} // namespace SlotSwitcherTest
