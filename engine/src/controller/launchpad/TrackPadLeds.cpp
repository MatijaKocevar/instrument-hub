#include "controller/launchpad/TrackPadLeds.h"

namespace Controller {

namespace {
constexpr PatchLed offLed() {
    return {Launchpad::velocityFor(Launchpad::Colour::Off), Launchpad::LedMode::Static};
}

constexpr PatchLed colourLed(Launchpad::Colour colour) {
    return {Launchpad::velocityFor(colour), Launchpad::LedMode::Static};
}

constexpr std::uint8_t volumeVelocity(int step) noexcept {
    switch (step) {
        case 0:
        case 1:
            return 0x10;
        case 2:
            return 0x13;
        case 3:
            return 0x18;
        case 4:
            return 0x20;
        default:
            return 0x13;
    }
}
}  // namespace

const TrackLedState* TrackLedSnapshot::trackForSlot(int slot) const noexcept {
    for (const auto& track : tracks)
        if (track.slot == slot) return &track;

    return nullptr;
}

PatchLed launchpadLedForTrackPad(const PadBinding& binding,
                                 const TrackLedSnapshot& snapshot) noexcept {
    const auto* track = snapshot.trackForSlot(binding.slot);

    if (track == nullptr) return offLed();

    switch (binding.role) {
        case PadRole::track:
            if (!track->hasInstrument) return offLed();

            if (track->selected || track->slot == snapshot.armedSlot)
                return colourLed(Launchpad::Colour::Red);

            return {Launchpad::velocityForTrackColour(track->colorIndex),
                    Launchpad::LedMode::Static};

        case PadRole::fx:
            if (!track->hasInstrument) return offLed();

            if (binding.hot < 0 || binding.hot >= SetMapping::maximumHotFx) return offLed();

            {
                const auto& hot = track->hotFx[static_cast<std::size_t>(binding.hot)];

                if (!hot.assigned) return offLed();

                return colourLed(hot.enabled ? Launchpad::Colour::Green
                                             : Launchpad::Colour::Yellow);
            }

        case PadRole::volume:
            if (!track->hasInstrument || binding.step > track->volumeStep) return offLed();

            return {volumeVelocity(binding.step), Launchpad::LedMode::Static};
    }

    return offLed();
}

}  // namespace Controller
