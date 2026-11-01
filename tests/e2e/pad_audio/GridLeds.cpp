#include "pad_audio/GridLeds.h"

namespace PadAudio
{

std::map<int, LedHit> captureGridLeds (const Api::StateSnapshot& snapshot,
                                       Controller::LaunchpadDriver& driver)
{
    std::map<int, LedHit> hits;

    driver.setMessageSink ([&hits] (const juce::MidiMessage& message)
    {
        if (! message.isNoteOn())
            return;

        const int pad = Launchpad::padIndexForNote (message.getNoteNumber());

        if (pad < 0)
            return;

        LedHit& hit = hits[pad];
        hit.seen = true;
        hit.velocity = static_cast<std::uint8_t> (message.getVelocity());
        hit.mode = message.getChannel() - 1;
    });

    std::map<int, const Api::PatchState*> patches;

    for (const auto& family : snapshot.families)
        for (const auto& patch : family.patches)
            if (patch.slot >= 0 && patch.slot < Launchpad::padCount)
                patches[patch.slot] = &patch;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const auto found = patches.find (pad);

        if (found == patches.end())
        {
            driver.setPadLed (pad, {});
            continue;
        }

        const auto& patch = *found->second;
        driver.setPatchLed (pad,
                            patch.colorIndex,
                            pad == snapshot.armedSlot,
                            patch.warmState == "warming",
                            patch.warmState == "error");
    }

    driver.setMessageSink (nullptr);
    return hits;
}

} // namespace PadAudio
