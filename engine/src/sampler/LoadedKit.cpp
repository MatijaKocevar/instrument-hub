#include "LoadedKit.h"

#include <juce_core/juce_core.h>

namespace ihub::sampler {

int LoadedKit::findPadBinding(int note) const noexcept {
  for (std::size_t i = 0; i < padBindings.size(); ++i)
    if (padBindings[i].note == note) return (int)i;

  return -1;
}

ResolvedHit LoadedKit::makeHit(std::size_t layerBindingIndex, int variant,
                               int note, int velocity) const noexcept {
  if (layerBindingIndex >= layerBindings.size()) return {};

  const auto& binding = layerBindings[layerBindingIndex];

  if (binding.padIndex >= kit.pads.size() || binding.sampleCount == 0)
    return {};

  const auto& pad = kit.pads[binding.padIndex];

  if (binding.layerIndex >= pad.layers.size()) return {};

  const auto& layer = pad.layers[binding.layerIndex];
  const auto variantIndex =
      (std::size_t)(variant < 0 ? 0 : variant) % binding.sampleCount;

  if (binding.firstSample + variantIndex >= samplePointers.size()) return {};

  const auto* sample = samplePointers[binding.firstSample + variantIndex];
  const auto& sampleRef = layer.roundRobin[variantIndex];

  ResolvedHit hit;
  hit.sample = sample;
  hit.pad = &pad;
  hit.layer = &layer;
  hit.note = note;
  hit.velocity = velocity;
  hit.variant = (int)variantIndex;
  hit.gain = pad.gain * sampleRef.gain;
  hit.pan = juce::jlimit(-1.0f, 1.0f, pad.pan + sampleRef.pan);
  hit.playbackRatio = pad.playbackRatio() * sampleRef.playbackRatio();

  return hit;
}

ResolvedHit LoadedKit::resolve(int note, int velocity) noexcept {
  const auto padIndex = findPadBinding(note);

  if (padIndex < 0) return {};

  const auto& padBinding = padBindings[(std::size_t)padIndex];

  for (std::size_t i = 0; i < padBinding.numLayers; ++i) {
    const auto layerIndex = padBinding.firstLayer + i;

    if (layerIndex >= layerBindings.size()) break;

    const auto& binding = layerBindings[layerIndex];
    const auto& layer = kit.pads[binding.padIndex].layers[binding.layerIndex];

    if (!layer.contains(velocity)) continue;

    if (roundRobinCounters == nullptr || layerIndex >= numRoundRobinCounters)
      return {};

    const auto count =
        roundRobinCounters[layerIndex].fetch_add(1, std::memory_order_relaxed);
    return makeHit(layerIndex,
                   (int)(count % (std::uint32_t)binding.sampleCount), note,
                   velocity);
  }

  return {};
}

ResolvedHit LoadedKit::resolveWithVariant(int note, int velocity,
                                          int variantIndex) const noexcept {
  const auto padIndex = findPadBinding(note);

  if (padIndex < 0) return {};

  const auto& padBinding = padBindings[(std::size_t)padIndex];

  for (std::size_t i = 0; i < padBinding.numLayers; ++i) {
    const auto layerIndex = padBinding.firstLayer + i;

    if (layerIndex >= layerBindings.size()) break;

    const auto& binding = layerBindings[layerIndex];
    const auto& layer = kit.pads[binding.padIndex].layers[binding.layerIndex];

    if (layer.contains(velocity))
      return makeHit(layerIndex, variantIndex, note, velocity);
  }

  return {};
}

}  // namespace ihub::sampler
