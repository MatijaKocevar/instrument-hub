#include "ControllerSurface.h"

#include "api/MidiBridge.h"
#include "api/Server.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <array>
#include <iostream>
#include <utility>

namespace ihub::app
{

ControllerSurface::ControllerSurface() = default;

ControllerSurface::~ControllerSurface()
{
    resetMapping();
    shutdown();
}

/**
    Starts the Launchpad and the set.yaml mapping.

    Pads arm the slot their binding names, the API state mirrors that selection
    for the UI, and the LED of the selected pad turns green while every other
    pad goes dark. Diagnostic and snapshot modes never call this, so a test run
    never switches a connected controller out of Live mode.
*/
void ControllerSurface::start (Api::StateStore* state, Api::MidiBridge* bridge)
{
    apiState = state;
    midiBridge = bridge;

    const auto setFile = Controller::Mapping::findDefaultSetFile();

    if (! setFile.empty())
    {
        mapping = std::make_unique<Controller::Mapping>();

        Controller::Mapping::Listeners mappingListeners;

        mappingListeners.armSlot = [this] (int slot)
        {
            if (slot >= 0 && apiState != nullptr)
            {
                (void) apiState->setArmedSlot (slot);
                refreshLeds();
            }
        };

        mappingListeners.padEvent = [this] (int,
                                            const Controller::PadBinding*,
                                            bool)
        {
            // The armSlot listener above already refreshed the surface.
            // A release re-paints from the same state so a dropped press
            // cannot leave a stale LED.
            refreshLeds();
        };

        // The top row selects a macro page, the right column a scene. With
        // no encoders these LEDs are the only feedback the surface gives
        // for those selections.
        mappingListeners.bankEvent = [this] (int bank,
                                             const Controller::MacroBankSpec*)
        {
            if (launchpad == nullptr)
                return;

            for (int index = 0; index < Launchpad::topRowCount; ++index)
                launchpad->setButtonState (
                    Launchpad::topRowCCForIndex (index),
                    index == bank ? Controller::PadState::active
                                  : Controller::PadState::idle);
        };

        mappingListeners.sceneEvent = [this] (int scene,
                                              const Controller::SceneSpec*)
        {
            if (launchpad == nullptr)
                return;

            for (int index = 0; index < Launchpad::rightColumnCount; ++index)
                launchpad->setButtonState (
                    Launchpad::rightColumnCCForIndex (index),
                    index == scene ? Controller::PadState::active
                                   : Controller::PadState::idle);
        };

        mapping->setListeners (std::move (mappingListeners));

        Controller::MappingError error;

        if (! mapping->load (setFile, &error))
            std::cerr << "mapping: " << error.message << std::endl;
        else
            mapping->startWatching();
    }

    launchpad = std::make_unique<Controller::LaunchpadDriver>();

    if (midiBridge != nullptr)
        midiBridge->setLaunchpad (launchpad.get());

    Controller::LaunchpadDriver::DriverListeners driverListeners;

    driverListeners.padEvent = [this] (int pad, bool pressed)
    {
        if (mapping != nullptr)
            mapping->handlePad (pad, pressed);
    };

    driverListeners.buttonEvent = [this] (int cc, bool pressed)
    {
        if (mapping != nullptr)
            mapping->handleButton (cc, pressed);
    };

    launchpad->setListeners (std::move (driverListeners));

    if (! launchpad->connect())
        std::cerr << "controller: " << launchpad->getLastError() << std::endl;
    else
        refreshLeds();
}

/**
    Mirrors the `/state` grid onto the Launchpad LEDs.

    Unarmed patches show their palette colour, the armed patch is bright
    red, a warming patch is yellow and a load error flashes red. Called
    after every controller/API change and by the 100 ms poll, so an HTTP
    colour edit reaches the hardware without a reconnect.
*/
void ControllerSurface::refreshLeds()
{
    if (launchpad == nullptr || ! launchpad->isConnected() || apiState == nullptr)
        return;

    // The pad test owns the surface until it clears it; repainting the
    // grid here would overwrite the animation within 100 ms.
    if (midiBridge != nullptr && midiBridge->isPadTestActive())
        return;

    const auto snapshot = apiState->snapshot();
    std::array<const Api::PatchState*, Launchpad::padCount> patches {};

    for (const auto& family : snapshot.families)
    {
        for (const auto& patch : family.patches)
        {
            if (patch.slot < 0 || patch.slot >= Launchpad::padCount)
                continue;

            patches[static_cast<std::size_t> (patch.slot)] = &patch;
        }
    }

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const auto* patch = patches[static_cast<std::size_t> (pad)];

        if (patch == nullptr)
        {
            launchpad->setPadLed (pad, {});
            continue;
        }

        const bool armed = pad == snapshot.armedSlot;
        const bool warming = patch->warmState == "warming";
        const bool error = patch->warmState == "error";

        launchpad->setPatchLed (pad, patch->colorIndex, armed, warming, error);
    }
}

void ControllerSurface::resetMapping()
{
    mapping.reset();
}

void ControllerSurface::shutdown()
{
    if (launchpad != nullptr)
    {
        launchpad->disconnect();
        launchpad.reset();
    }
}

} // namespace ihub::app
