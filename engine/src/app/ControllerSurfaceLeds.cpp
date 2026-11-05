#include "ControllerSurface.h"

#include "api/MidiBridge.h"
#include "api/Server.h"
#include "app/surface/TrackSurface.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <array>

namespace ihub::app
{

void ControllerSurface::refreshLeds()
{
    if (launchpad == nullptr || ! launchpad->isConnected() || apiState == nullptr)
        return;

    if (midiBridge != nullptr && midiBridge->isPadTestActive())
        return;

    const auto snapshot = apiState->snapshot();
    const auto setMapping = mapping != nullptr ? mapping->getMapping() : nullptr;

    if (snapshot.layout == "tracks")
    {
        if (setMapping != nullptr)
            paintTrackLeds (*launchpad, *setMapping,
                            trackLedSnapshotFor (snapshot));

        return;
    }

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
        const bool warming = armed && patch->warmState == "warming";
        const bool error = patch->warmState == "error";

        launchpad->setPatchLed (pad, patch->colorIndex, armed, warming, error);
    }
}

}
