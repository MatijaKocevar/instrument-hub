#include "TrackBridge.h"

#include "StateStore.h"

#include "Session.h"

#include <algorithm>
#include <utility>

namespace Api {

void attachTrackState(StateStore& state, ihub::Session& session) {
  state.setTrackGainHandler(
      [&session](int slot, float gain) { session.setSlotGain(slot, gain); });

  const auto& config = session.getConfig();

  std::vector<TrackConfig> tracks;
  tracks.reserve(config.tracks.empty() ? 8 : config.tracks.size());

  if (!config.tracks.empty()) {
    for (const auto& source : config.tracks) {
      if (source.slot < 0 ||
          source.slot >= static_cast<int>(config.slots.size()))
        continue;

      TrackConfig track;
      track.id = source.id;
      track.name =
          source.name.empty()
              ? config.slots[static_cast<std::size_t>(source.slot)].name
              : source.name;
      track.slot = source.slot;
      track.hotFx = source.hotFx;
      track.gain = config.slots[static_cast<std::size_t>(source.slot)].gain;
      tracks.push_back(std::move(track));
    }
  } else {
    const int count = juce::jmin(8, static_cast<int>(config.slots.size()));

    for (int slot = 0; slot < count; ++slot) {
      TrackConfig track;
      track.id = "track-" + std::to_string(slot + 1);
      track.name = config.slots[static_cast<std::size_t>(slot)].name;
      track.slot = slot;
      track.gain = config.slots[static_cast<std::size_t>(slot)].gain;
      tracks.push_back(std::move(track));
    }
  }

  std::sort(tracks.begin(), tracks.end(),
            [](const TrackConfig& left, const TrackConfig& right) {
              return left.slot < right.slot;
            });

  state.setTracks(std::move(tracks));
}

}  // namespace Api
