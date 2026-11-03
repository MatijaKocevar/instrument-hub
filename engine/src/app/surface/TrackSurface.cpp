#include "app/surface/TrackSurface.h"

#include "api/StateStore.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"
#include "controller/launchpad/TrackPadRouting.h"

#include <string>

namespace ihub::app
{

int trackIndexForSlot (const Api::StateSnapshot& snapshot, int slot) noexcept
{
    for (std::size_t index = 0; index < snapshot.tracks.size(); ++index)
        if (snapshot.tracks[index].slot == slot)
            return static_cast<int> (index);

    return -1;
}

Controller::TrackLedSnapshot trackLedSnapshotFor (const Api::StateSnapshot& snapshot,
                                                  const Api::StateStore& state)
{
    Controller::TrackLedSnapshot result;
    result.armedSlot = snapshot.armedSlot;
    result.tracks.reserve (snapshot.tracks.size());

    for (const auto& source : snapshot.tracks)
    {
        Controller::TrackLedState track;
        track.slot = source.slot;
        track.selected = source.selected;
        track.volumeStep = source.volumeStep;
        track.colorIndex = state.getPatch (source.slot).colorIndex;

        for (std::size_t hot = 0;
             hot < track.hotFx.size() && hot < source.hotFx.size();
             ++hot)
        {
            const auto& entry = source.hotFx[hot];
            track.hotFx[hot].assigned = ! entry.id.empty();
            track.hotFx[hot].enabled = ! entry.bypassed;
        }

        result.tracks.push_back (track);
    }

    return result;
}

void applyTrackPadAction (Api::StateStore& state,
                          const Controller::PadBinding& binding)
{
    if (binding.role != Controller::PadRole::fx
        && binding.role != Controller::PadRole::volume)
        return;

    const auto snapshot = state.snapshot();
    const int index = trackIndexForSlot (snapshot, binding.slot);

    if (index < 0)
        return;

    std::string error;

    if (binding.role == Controller::PadRole::fx)
    {
        (void) state.toggleTrackFx (index, binding.hot, error);
        return;
    }

    const auto action = Controller::trackPadActionFor (
        binding, snapshot.tracks[static_cast<std::size_t> (index)].volumeStep);

    if (action.has_value())
        (void) state.setTrackVolumeStep (index, action->step, error);
}

void paintTrackLeds (Controller::LaunchpadDriver& driver,
                     const Controller::SetMapping& mapping,
                     const Controller::TrackLedSnapshot& snapshot)
{
    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const auto* binding = mapping.padBinding (pad);

        if (binding == nullptr)
        {
            driver.setPadLed (pad, {});
            continue;
        }

        driver.setPadLed (pad, Controller::launchpadLedForTrackPad (*binding, snapshot));
    }
}

}
