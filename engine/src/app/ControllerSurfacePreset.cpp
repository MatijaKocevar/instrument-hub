#include "ControllerSurface.h"

#include "api/MidiBridge.h"
#include "api/StateStore.h"
#include "controller/LaunchpadDriver.h"
#include "controller/LaunchpadGrid.h"
#include "controller/Mapping.h"
#include "controller/PadState.h"

namespace ihub::app
{

namespace
{

constexpr int presetPreviousCC = 29;
constexpr int presetNextCC = 28;
constexpr int categoryPreviousCC = 20;
constexpr int categoryNextCC = 21;

bool armedTrackHasInstrument (const Api::StateSnapshot& snapshot)
{
    for (const auto& track : snapshot.tracks)
        if (track.slot == snapshot.armedSlot)
            return track.instrument.kind != "empty";

    return false;
}

}

bool ControllerSurface::handleBrowseButton (int cc, bool pressed)
{
    if (apiState == nullptr || midiBridge == nullptr)
        return false;

    const auto topRow = Launchpad::topRowIndexForCC (cc);

    if (! topRow.has_value())
        return false;

    if (mapping != nullptr)
    {
        const auto setMapping = mapping->getMapping();

        if (setMapping != nullptr && setMapping->bankBinding (*topRow) != nullptr)
            return false;
    }

    int controllerNumber = -1;
    bool changesPreset = false;

    switch (*topRow)
    {
        case categoryPreviousButton:
            controllerNumber = categoryPreviousCC;
            break;

        case categoryNextButton:
            controllerNumber = categoryNextCC;
            break;

        case presetPreviousButton:
            controllerNumber = presetPreviousCC;
            changesPreset = true;
            break;

        case presetNextButton:
            controllerNumber = presetNextCC;
            changesPreset = true;
            break;

        default:
            return false;
    }

    const auto snapshot = apiState->snapshot();

    if (snapshot.layout != "tracks" || ! armedTrackHasInstrument (snapshot))
        return false;

    const auto message = juce::MidiMessage::controllerEvent (
        1, controllerNumber, pressed ? 127 : 0);

    if (launchpad != nullptr)
    {
        if (changesPreset)
        {
            launchpad->setButtonState (
                cc, pressed ? Controller::PadState::active
                            : Controller::PadState::warming);
        }
        else
        {
            launchpad->setButtonState (
                cc, pressed ? Controller::PadState::warming
                            : Controller::PadState::active);
        }
    }

    if (! midiBridge->sendToSession (message))
        return false;

    if (pressed && changesPreset)
        apiState->notePluginStateEdit (snapshot.armedSlot);

    return true;
}

}
