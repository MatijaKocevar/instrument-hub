#include "api/StateStore.h"

#include <array>
#include <cmath>
#include <utility>

namespace Api {

namespace {
const std::array<double, trackVolumeMaximumStep + 1> stepGains{
    0.0, 0.5, 0.75, 1.0, trackVolumeMaximumGain};
}

double trackGainForStep(int step) noexcept {
  if (step < trackVolumeMinimumStep || step > trackVolumeMaximumStep)
    return 1.0;

  return stepGains[static_cast<std::size_t>(step)];
}

double trackDbForGain(double gain) noexcept {
  if (gain <= 0.0) return -60.0;

  return 20.0 * std::log10(gain);
}

int trackVolumeStepForGain(double gain) noexcept {
  if (gain <= 0.0) return trackVolumeMinimumStep;

  int step = trackVolumeMinimumStep;

  for (int candidate = trackVolumeMinimumStep + 1;
       candidate <= trackVolumeMaximumStep; ++candidate) {
    if (gain + 1.0e-6 >= stepGains[static_cast<std::size_t>(candidate)])
      step = candidate;
  }

  return step;
}

void StateStore::setTrackGainHandler(TrackGainHandler handler) {
  const std::lock_guard<std::mutex> lock(mutex);
  trackGainHandler = std::move(handler);
}

bool StateStore::setTrackGain(int index, double gain, std::string& error) {
  if (!std::isfinite(gain) || gain < trackVolumeMinimumGain ||
      gain > trackVolumeMaximumGain) {
    error =
        "gain must be between 0 and " + std::to_string(trackVolumeMaximumGain);
    return false;
  }

  int slot = -1;
  TrackGainHandler handler;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (index < 0 || index >= static_cast<int>(tracks.size())) {
      error = "track index is out of range";
      return false;
    }

    auto& track = tracks[static_cast<std::size_t>(index)];

    if (track.slot < 0 || track.slot >= numSlots) {
      error = "track slot is out of range";
      return false;
    }

    track.gain = gain;
    slot = track.slot;
    slotMixes[static_cast<std::size_t>(slot)].volumeDb = trackDbForGain(gain);
    handler = trackGainHandler;
  }

  if (handler != nullptr) handler(slot, static_cast<float>(gain));

  notifyEdited("track", slot);
  notifyChanged("track", index);
  return true;
}

bool StateStore::setTrackVolumeStep(int index, int step, std::string& error) {
  if (step < trackVolumeMinimumStep || step > trackVolumeMaximumStep) {
    error = "step must be between 0 and 4";
    return false;
  }

  return setTrackGain(index, trackGainForStep(step), error);
}

}  // namespace Api
