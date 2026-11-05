#pragma once

#include "controller/launchpad/TrackPadLeds.h"

namespace Api
{
class StateStore;
struct StateSnapshot;
}

namespace Controller
{
class LaunchpadDriver;
class SetMapping;
struct PadBinding;
}

namespace ihub::app
{

int trackIndexForSlot (const Api::StateSnapshot& snapshot, int slot) noexcept;

Controller::TrackLedSnapshot trackLedSnapshotFor (const Api::StateSnapshot& snapshot);

void applyTrackPadAction (Api::StateStore& state,
                          const Controller::PadBinding& binding);

void paintTrackLeds (Controller::LaunchpadDriver& driver,
                     const Controller::SetMapping& mapping,
                     const Controller::TrackLedSnapshot& snapshot);

}
