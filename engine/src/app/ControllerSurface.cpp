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
            refreshLeds();
        };

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

void ControllerSurface::refreshLeds()
{
    if (launchpad == nullptr || ! launchpad->isConnected() || apiState == nullptr)
        return;

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

}
