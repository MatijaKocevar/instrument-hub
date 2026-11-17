#include "api/StateStore.h"

#include <algorithm>
#include <utility>

namespace Api {

StateSnapshot StateStore::snapshot() const {
  StateSnapshot result;
  TransportProvider transportReader;
  SlotRuntimeProvider slotReader;
  ProjectStateProvider projectReader;
  PluginPageProvider pageReader;
  std::vector<PatchInstrumentState> instruments;
  std::vector<PatchMixState> mixes;
  std::vector<bool> editors;
  std::vector<TrackConfig> trackConfigs;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    result.revision = revision.load(std::memory_order_acquire);
    result.layout = layout;
    result.armedSlot = armedSlot;
    result.settings = settings;
    result.transport = transport;
    result.slots.reserve(static_cast<std::size_t>(numSlots));

    for (int index = 0; index < numSlots; ++index) {
      const auto position = static_cast<std::size_t>(index);

      SlotSnapshot slot;
      slot.index = index;
      slot.name = slotNames[position];
      slot.armed = index == armedSlot;
      slot.family = slotFamilies[position];
      slot.familyName = slotFamilyNames[position];
      slot.patch = slotPatches[position];
      slot.patchName = slotPatchNames[position];
      slot.latencySamples = slotLatencySamples[position];
      slot.latencyMs = slotLatencyMs[position];
      result.slots.push_back(std::move(slot));
    }

    result.families = families;
    instruments = slotInstruments;
    mixes = slotMixes;
    editors = slotEditorOpen;
    trackConfigs = tracks;

    transportReader = transportProvider;
    slotReader = slotRuntimeProvider;
    projectReader = projectStateProvider;
    pageReader = pluginPageProvider;
  }

  if (transportReader != nullptr) {
    try {
      result.transport = transportReader();
    } catch (...) {
    }
  }

  if (slotReader != nullptr) {
    for (auto& slot : result.slots) {
      try {
        const auto runtime = slotReader(slot.index);
        slot.armed = runtime.armed;
        slot.gain = runtime.gain;
        slot.state = runtime.state;
        slot.warmState = runtime.warmState;
        slot.warm = runtime.warm;
        slot.editorOpen = runtime.editorOpen;
        slot.error = runtime.error;
      } catch (...) {
      }
    }
  }

  if (projectReader != nullptr) {
    try {
      result.project = projectReader();
    } catch (...) {
    }
  }

  if (pageReader != nullptr) {
    try {
      result.pages = pageReader();
    } catch (...) {
    }
  }

  result.tracks.reserve(trackConfigs.size());

  for (std::size_t index = 0; index < trackConfigs.size(); ++index) {
    const auto& config = trackConfigs[index];

    TrackState track;
    track.index = static_cast<int>(index);
    track.id = config.id;
    track.name = config.name;
    track.slot = config.slot;
    track.volumeStep = trackVolumeStepForGain(config.gain);
    track.volumeDb = trackDbForGain(config.gain);
    track.gain = config.gain;
    track.hotFx.resize(2);

    for (std::size_t hot = 0; hot < track.hotFx.size(); ++hot)
      track.hotFx[hot].index = static_cast<int>(hot);

    if (config.slot >= 0 &&
        config.slot < static_cast<int>(result.slots.size())) {
      const auto slotIndex = static_cast<std::size_t>(config.slot);
      track.selected = result.slots[slotIndex].armed;
      track.instrument = instruments[slotIndex];

      if (track.instrument.name.empty()) track.instrument.name = track.name;
    }

    for (std::size_t hot = 0;
         hot < track.hotFx.size() && hot < config.hotFx.size(); ++hot) {
      auto& entry = track.hotFx[hot];
      entry.id = config.hotFx[hot];
      entry.name = entry.id;
    }

    result.tracks.push_back(std::move(track));
  }

  for (auto& track : result.tracks) {
    const bool assigned =
        std::any_of(track.hotFx.begin(), track.hotFx.end(),
                    [](const TrackHotFxState& hot) { return !hot.id.empty(); });

    if (!assigned) continue;

    FxChainState chain;
    std::string error;

    if (!getFxChain(track.slot, chain, error)) continue;

    for (auto& hot : track.hotFx) {
      if (hot.id.empty()) continue;

      const auto insert =
          std::find_if(chain.inserts.begin(), chain.inserts.end(),
                       [&hot](const FxInsertState& candidate) {
                         return candidate.id == hot.id;
                       });

      if (insert == chain.inserts.end()) {
        hot.id.clear();
        hot.name.clear();
        hot.bypassed = false;
        continue;
      }

      hot.name = insert->name.empty() ? insert->id : insert->name;
      hot.bypassed = insert->bypassed;
    }
  }

  for (auto& family : result.families) {
    for (auto& patch : family.patches) {
      const int slot = patch.slot;

      if (slot < 0 || slot >= static_cast<int>(mixes.size()) ||
          slot >= static_cast<int>(result.slots.size()))
        continue;

      const auto index = static_cast<std::size_t>(slot);
      const auto& mix = mixes[index];
      const auto& live = result.slots[index];

      patch.instrument = instruments[index];
      patch.volumeDb = mix.volumeDb;
      patch.pan = mix.pan;
      patch.mute = mix.mute;
      patch.solo = mix.solo;
      patch.colorIndex = mix.colorIndex;

      if (patch.instrument.name.empty()) patch.instrument.name = patch.name;

      patch.warmState = live.warmState;
      patch.warm = live.warm;
      patch.editorOpen = slotReader != nullptr
                             ? live.editorOpen
                             : (index < editors.size() && editors[index]);
      patch.error = live.error;
      patch.latencySamples = live.latencySamples;
      patch.latencyMs = live.latencyMs;
    }
  }

  result.uptimeMs = getUptimeMs();
  return result;
}

}  // namespace Api
