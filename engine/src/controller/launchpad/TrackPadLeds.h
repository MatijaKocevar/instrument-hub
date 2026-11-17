#pragma once

#include "controller/LaunchpadLed.h"
#include "controller/MappingModel.h"

#include <array>
#include <vector>

namespace Controller {

struct TrackHotFxLed {
  bool assigned = false;
  bool enabled = true;
};

struct TrackLedState {
  int slot = -1;
  int colorIndex = 0;
  bool selected = false;
  bool hasInstrument = true;
  int volumeStep = SetMapping::maximumVolumeStep;
  std::array<TrackHotFxLed, SetMapping::maximumHotFx> hotFx{};
};

struct TrackLedSnapshot {
  int armedSlot = -1;
  std::vector<TrackLedState> tracks;

  const TrackLedState* trackForSlot(int slot) const noexcept;
};

PatchLed launchpadLedForTrackPad(const PadBinding& binding,
                                 const TrackLedSnapshot& snapshot) noexcept;

}  // namespace Controller
