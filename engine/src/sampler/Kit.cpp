#include "Kit.h"

#include <algorithm>

namespace ihub::sampler {

const VelocityLayer* KitPad::layerForVelocity(int velocity) const noexcept {
  for (const auto& layer : layers)
    if (layer.contains(velocity)) return &layer;

  return nullptr;
}

const KitPad* Kit::findPad(int note) const noexcept {
  for (const auto& pad : pads)
    if (pad.note == note) return &pad;

  return nullptr;
}

bool isSafeSamplePath(std::string_view path) noexcept {
  if (path.empty() || path.size() > maximumKitSamplePathBytes) return false;

  std::size_t segmentStart = 0;

  for (std::size_t i = 0; i <= path.size(); ++i) {
    const auto character = i < path.size() ? path[i] : '/';

    if (character == '\0') return false;

    if (character == '/' || character == '\\') {
      const auto segment = path.substr(segmentStart, i - segmentStart);

      if (segment == "..") return false;

      segmentStart = i + 1;
    }
  }

  return std::any_of(path.begin(), path.end(), [](char character) {
    return character != '/' && character != '\\' && character != ' ' &&
           character != '\t';
  });
}

}  // namespace ihub::sampler
