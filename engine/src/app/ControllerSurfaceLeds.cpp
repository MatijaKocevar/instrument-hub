#include "ControllerSurface.h"

#include "api/MidiBridge.h"
#include "api/Server.h"
#include "app/surface/TrackSurface.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

namespace ihub::app
{

    void ControllerSurface::refreshLeds ()
    {
        if (launchpad == nullptr || !launchpad->isConnected () || apiState == nullptr)
            return;

        const auto snapshot = apiState->snapshot ();
        const auto setMapping = mapping != nullptr ? mapping->getMapping () : nullptr;

        if (setMapping != nullptr)
            paintTrackLeds (*launchpad, *setMapping, trackLedSnapshotFor (snapshot));

        bool hasInstrument = false;

        for (const auto& track : snapshot.tracks)
            if (track.slot == snapshot.armedSlot)
                hasInstrument = track.instrument.kind != "empty";

        const auto setButton = [&] (int index, Controller::PadState state)
        {
            if (setMapping != nullptr && setMapping->bankBinding (index) != nullptr)
                return;

            launchpad->setButtonState (Launchpad::topRowCCForIndex (index), state);
        };

        const auto presetState =
            hasInstrument ? Controller::PadState::warming : Controller::PadState::idle;
        const auto categoryState =
            hasInstrument ? Controller::PadState::active : Controller::PadState::idle;

        setButton (presetPreviousButton, presetState);
        setButton (presetNextButton, presetState);
        setButton (categoryPreviousButton, categoryState);
        setButton (categoryNextButton, categoryState);
    }

}
