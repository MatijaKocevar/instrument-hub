#include "ProjectSession.h"

#include "SessionConfig.h"
#include "SessionSlotMapping.h"

#include "controller/MappingYaml.h"
#include "project/Serializer.h"

#include <utility>

namespace ihub::app {

bool loadProjectSession(const juce::File& projectFile, SessionConfig& config,
                        juce::String& error) {
  project::SetFile set;

  if (!project::Serializer::load(projectFile, set, error)) return false;

  Controller::SetMapping document;
  Controller::MappingError mappingError;

  if (!Controller::parseSetMappingYaml(set.mappingYaml, document,
                                       mappingError)) {
    error = "project mapping: line " + juce::String(mappingError.line) + ": " +
            juce::String(mappingError.message);
    return false;
  }

  config.sharedPluginHost = true;
  config.warmSlots.clear();
  config.tracks.clear();

  {
    config.slots.assign(Controller::SetMapping::maximumTracks,
                        SessionSlotConfig{});

    config.tracks.reserve(document.tracks.size());

    for (const auto& track : document.tracks) {
      SessionTrackConfig trackConfig;
      trackConfig.id = track.id;
      trackConfig.name = track.name;
      trackConfig.slot = track.slot;
      trackConfig.hotFx = track.hotFx;
      config.tracks.push_back(std::move(trackConfig));

      if (track.slot < 0 || track.slot >= Controller::SetMapping::maximumTracks)
        continue;

      auto& slot = config.slots[static_cast<std::size_t>(track.slot)];
      slot = SessionSlotConfig{};
      slot.name = track.name;
      slot.colorIndex = 14;

      config.warmSlots.push_back(track.slot);
    }
  }

  for (const auto& source : set.slots) {
    if (source.index < 0 ||
        source.index >= Controller::SetMapping::maximumTracks)
      continue;

    auto& slot = config.slots[static_cast<std::size_t>(source.index)];
    applyProjectSlot(source, slot);

    if (slot.kind != SessionSlotKind::plugin ||
        source.instrument.pluginState.empty())
      continue;

    const auto* chunk = set.findPluginState(source.instrument.pluginState);

    if (chunk == nullptr) {
      error = "slot " + juce::String(source.index) +
              " references missing plugin state " +
              juce::String(source.instrument.pluginState);
      return false;
    }

    slot.pluginState = chunk->data;

    if (slot.pluginFormat.empty()) slot.pluginFormat = chunk->format;

    if (slot.pluginName.empty()) slot.pluginName = chunk->name;
  }

  error.clear();
  return true;
}

}  // namespace ihub::app
