#include "Disarm.h"
#include "Fixtures.h"

#include "audio/SlotSwitcher.h"

#include <iostream>

namespace SlotSwitcherTest
{

void runDisarm()
{
    std::cout << "\n-- disarm fades the armed slot out --" << std::endl;

    SineSource sourceA (kAmplitude, kFrequency, 0.0);
    AudioEngine::Slot slotA (0);
    slotA.setSource (&sourceA);

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slotA });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::MidiBuffer midi;
    switcher.armSlot (0);

    for (int block = 0; block < kPreRollBlocks; ++block)
        switcher.process (kBlockSize, midi);

    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::active,
           "slot 0 is active before disarm");

    switcher.disarm();
    check (switcher.getArmedSlot() == -1, "disarm clears the request");

    switcher.process (kBlockSize, midi);
    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::fading,
           "the slot fades out after disarm");

    for (int block = 0; block < 2; ++block)
        switcher.process (kBlockSize, midi);

    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::idle,
           "the slot is idle after the fade completes");
}

}
