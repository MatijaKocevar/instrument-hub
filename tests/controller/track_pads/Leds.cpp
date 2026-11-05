#include "Leds.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"
#include "controller/launchpad/TrackPadLeds.h"

#include <string>

namespace TrackPadTest
{

using ControllerTest::check;

namespace
{

Controller::PadBinding bindingFor (int pad, Controller::PadRole role)
{
    Controller::PadBinding binding;
    binding.pad = pad;
    binding.role = role;
    binding.slot = pad % 8;
    binding.step = pad / 8;
    binding.hot = pad >= 48 ? 0 : (pad >= 40 ? 1 : -1);
    return binding;
}

bool isColour (const Controller::PatchLed& led, Launchpad::Colour colour)
{
    return led.velocity == Launchpad::velocityFor (colour)
           && led.mode == Launchpad::LedMode::Static;
}

}

void runLedChecks()
{
    Controller::TrackLedSnapshot snapshot;
    snapshot.armedSlot = 1;

    for (int slot = 0; slot < 8; ++slot)
    {
        Controller::TrackLedState track;
        track.slot = slot;
        track.colorIndex = slot;
        track.selected = slot == 1;
        track.volumeStep = slot == 0 ? 2 : (slot == 2 ? 4 : 3);

        if (slot == 0)
        {
            track.hotFx[0].assigned = true;
            track.hotFx[0].enabled = true;
            track.hotFx[1].assigned = true;
            track.hotFx[1].enabled = false;
        }

        snapshot.tracks.push_back (track);
    }

    const auto armed = Controller::launchpadLedForTrackPad (
        bindingFor (57, Controller::PadRole::track), snapshot);
    check (isColour (armed, Launchpad::Colour::Red),
           "the armed track pad is red");

    const auto idle = Controller::launchpadLedForTrackPad (
        bindingFor (56, Controller::PadRole::track), snapshot);
    check (idle.velocity == Launchpad::velocityForTrackColour (0),
           "an unarmed track pad keeps its instrument colour");

    const auto enabledFx = Controller::launchpadLedForTrackPad (
        bindingFor (48, Controller::PadRole::fx), snapshot);
    check (isColour (enabledFx, Launchpad::Colour::Green),
           "an enabled hot insert is green");

    const auto disabledFx = Controller::launchpadLedForTrackPad (
        bindingFor (40, Controller::PadRole::fx), snapshot);
    check (isColour (disabledFx, Launchpad::Colour::Yellow),
           "a bypassed hot insert is yellow");

    const auto unassignedFx = Controller::launchpadLedForTrackPad (
        bindingFor (49, Controller::PadRole::fx), snapshot);
    check (isColour (unassignedFx, Launchpad::Colour::Off),
           "an unassigned hot pad is dark");

    const auto boostVolume = Controller::launchpadLedForTrackPad (
        bindingFor (34, Controller::PadRole::volume), snapshot);
    check (boostVolume.velocity == 0x20
               && boostVolume.mode == Launchpad::LedMode::Static,
           "the +3 dB step lights its pad in the lightest green");

    const auto boostBelow = Controller::launchpadLedForTrackPad (
        bindingFor (26, Controller::PadRole::volume), snapshot);
    check (boostBelow.velocity == 0x18
               && boostBelow.mode == Launchpad::LedMode::Static,
           "the 100% step lights in a lighter green than the steps below");

    const auto activeVolume = Controller::launchpadLedForTrackPad (
        bindingFor (16, Controller::PadRole::volume), snapshot);
    check (activeVolume.velocity == 0x13
               && activeVolume.mode == Launchpad::LedMode::Static,
           "the 75% volume step has its own green shade");

    const auto lowerVolume = Controller::launchpadLedForTrackPad (
        bindingFor (8, Controller::PadRole::volume), snapshot);
    check (lowerVolume.velocity == 0x10
               && lowerVolume.mode == Launchpad::LedMode::Static,
           "the 50% volume step is a darker green");

    const auto inactiveVolume = Controller::launchpadLedForTrackPad (
        bindingFor (24, Controller::PadRole::volume), snapshot);
    check (isColour (inactiveVolume, Launchpad::Colour::Off),
           "a volume step above the current level is dark");

    int red = 0;

    for (int pad = 56; pad < 64; ++pad)
    {
        const auto led = Controller::launchpadLedForTrackPad (
            bindingFor (pad, Controller::PadRole::track), snapshot);

        if (isColour (led, Launchpad::Colour::Red))
            ++red;
    }

    check (red == 1, "the track row has exactly one red LED");
}

}
