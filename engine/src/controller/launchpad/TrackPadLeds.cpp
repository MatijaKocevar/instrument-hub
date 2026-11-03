#include "controller/launchpad/TrackPadLeds.h"

namespace Controller
{

namespace
{
constexpr PatchLed offLed()
{
    return { Launchpad::velocityFor (Launchpad::Colour::Off),
             Launchpad::LedMode::Static };
}

constexpr PatchLed colourLed (Launchpad::Colour colour)
{
    return { Launchpad::velocityFor (colour), Launchpad::LedMode::Static };
}
}

const TrackLedState* TrackLedSnapshot::trackForSlot (int slot) const noexcept
{
    for (const auto& track : tracks)
        if (track.slot == slot)
            return &track;

    return nullptr;
}

PatchLed launchpadLedForTrackPad (const PadBinding& binding,
                                  const TrackLedSnapshot& snapshot) noexcept
{
    const auto* track = snapshot.trackForSlot (binding.slot);

    if (track == nullptr)
        return offLed();

    switch (binding.role)
    {
        case PadRole::patch:
        case PadRole::track:
            if (track->selected || track->slot == snapshot.armedSlot)
                return colourLed (Launchpad::Colour::Red);

            return { Launchpad::velocityForPatchColour (track->colorIndex),
                     Launchpad::LedMode::Static };

        case PadRole::fx:
            if (binding.hot < 0 || binding.hot >= SetMapping::maximumHotFx)
                return offLed();

            {
                const auto& hot = track->hotFx[static_cast<std::size_t> (binding.hot)];

                if (! hot.assigned)
                    return offLed();

                return colourLed (hot.enabled ? Launchpad::Colour::Green
                                              : Launchpad::Colour::Yellow);
            }

        case PadRole::volume:
            if (binding.step == SetMapping::maximumVolumeStep
                && track->volumeStep == SetMapping::maximumVolumeStep + 1)
                return colourLed (Launchpad::Colour::White);

            if (binding.step == track->volumeStep)
                return colourLed (Launchpad::Colour::Blue);

            return offLed();
    }

    return offLed();
}

}
