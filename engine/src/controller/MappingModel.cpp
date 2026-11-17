#include "MappingModel.h"

namespace Controller {

const PadBinding* SetMapping::padBinding(int pad) const noexcept {
  if (pad < 0 || pad >= SetMapping::maximumPads) return nullptr;

  const auto& entry = pads[static_cast<std::size_t>(pad)];
  return entry.has_value() ? &*entry : nullptr;
}

const EncoderBinding* SetMapping::encoderBinding(int encoder) const noexcept {
  if (encoder < 0 || encoder >= SetMapping::maximumEncoders) return nullptr;

  const auto& entry = encoders[static_cast<std::size_t>(encoder)];
  return entry.has_value() ? &*entry : nullptr;
}

const MacroBankSpec* SetMapping::bankBinding(int bank) const noexcept {
  if (bank < 0 || bank >= SetMapping::maximumBanks) return nullptr;

  const auto& entry = banks[static_cast<std::size_t>(bank)];
  return entry.has_value() ? &*entry : nullptr;
}

const SceneSpec* SetMapping::sceneBinding(int scene) const noexcept {
  if (scene < 0 || scene >= SetMapping::maximumScenes) return nullptr;

  const auto& entry = scenes[static_cast<std::size_t>(scene)];
  return entry.has_value() ? &*entry : nullptr;
}

const TrackSpec* SetMapping::trackAtSlot(int slot) const noexcept {
  for (const auto& track : tracks)
    if (track.slot == slot) return &track;

  return nullptr;
}

const TrackSpec* SetMapping::trackForId(std::string_view id) const noexcept {
  for (const auto& track : tracks)
    if (track.id == id) return &track;

  return nullptr;
}

const TrackSpec* SetMapping::trackFor(
    const PadBinding& binding) const noexcept {
  return binding.track.empty() ? nullptr : trackForId(binding.track);
}

const MacroSpec* SetMapping::macroFor(
    const EncoderBinding& binding) const noexcept {
  const auto macro = macros.find(binding.macro);
  return macro == macros.end() ? nullptr : &macro->second;
}

int SetMapping::countPads() const noexcept {
  int count = 0;

  for (const auto& entry : pads)
    if (entry.has_value()) ++count;

  return count;
}

int SetMapping::countEncoders() const noexcept {
  int count = 0;

  for (const auto& entry : encoders)
    if (entry.has_value()) ++count;

  return count;
}

int SetMapping::countBanks() const noexcept {
  int count = 0;

  for (const auto& entry : banks)
    if (entry.has_value()) ++count;

  return count;
}

int SetMapping::countScenes() const noexcept {
  int count = 0;

  for (const auto& entry : scenes)
    if (entry.has_value()) ++count;

  return count;
}

int SetMapping::countTracks() const noexcept {
  return static_cast<int>(tracks.size());
}

}  // namespace Controller
