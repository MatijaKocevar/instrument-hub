#include "FxChain.h"

#include <algorithm>
#include <cstddef>

namespace AudioEngine {

void FxChain::prepare(double newSampleRate, int newMaximumBlockSize,
                      int newNumChannels) {
  jassert(newSampleRate > 0.0);
  jassert(newMaximumBlockSize > 0);
  jassert(newNumChannels > 0);

  sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
  maximumBlockSize = juce::jmax(1, newMaximumBlockSize);
  numChannels = juce::jmax(1, newNumChannels);
  prepared = true;

  for (auto* processor : processors)
    if (processor != nullptr) prepareProcessor(*processor);

  refreshLatency();
}

void FxChain::releaseResources() {
  for (auto* processor : processors)
    if (processor != nullptr) processor->releaseResources();

  prepared = false;
  cachedLatency.store(0, std::memory_order_relaxed);
}

void FxChain::reset() noexcept {
  for (auto* processor : processors)
    if (processor != nullptr) processor->reset();
}

int FxChain::add(juce::AudioProcessor* processor) {
  if (processor == nullptr) return -1;

  processors.push_back(processor);

  if (prepared) {
    prepareProcessor(*processor);
    refreshLatency();
  }

  return static_cast<int>(processors.size()) - 1;
}

int FxChain::insert(int index, juce::AudioProcessor* processor) {
  if (processor == nullptr) return -1;

  const auto position =
      static_cast<std::size_t>(juce::jlimit(0, size(), index));

  processors.insert(processors.begin() + static_cast<std::ptrdiff_t>(position),
                    processor);

  if (prepared) {
    prepareProcessor(*processor);
    refreshLatency();
  }

  return static_cast<int>(position);
}

bool FxChain::remove(int index) noexcept {
  if (index < 0 || index >= size()) return false;

  processors.erase(processors.begin() + static_cast<std::ptrdiff_t>(index));
  refreshLatency();

  return true;
}

void FxChain::clear() noexcept {
  processors.clear();
  cachedLatency.store(0, std::memory_order_relaxed);
}

juce::AudioProcessor* FxChain::getProcessor(int index) const noexcept {
  if (index < 0 || index >= size()) return nullptr;

  return processors[static_cast<std::size_t>(index)];
}

int FxChain::indexOf(const juce::AudioProcessor* processor) const noexcept {
  for (std::size_t i = 0; i < processors.size(); ++i)
    if (processors[i] == processor) return static_cast<int>(i);

  return -1;
}

void FxChain::refreshLatency() noexcept {
  int total = 0;

  for (auto* processor : processors)
    if (processor != nullptr)
      total += juce::jmax(0, processor->getLatencySamples());

  cachedLatency.store(total, std::memory_order_relaxed);
}

void FxChain::prepareProcessor(juce::AudioProcessor& processor) {
  processor.setRateAndBufferSizeDetails(sampleRate, maximumBlockSize);

  const auto target = juce::AudioChannelSet::canonicalChannelSet(numChannels);

  auto layout = processor.getBusesLayout();
  bool changed = false;

  if (!layout.inputBuses.isEmpty() &&
      layout.inputBuses.getReference(0) != target) {
    layout.inputBuses.getReference(0) = target;
    changed = true;
  }

  if (!layout.outputBuses.isEmpty() &&
      layout.outputBuses.getReference(0) != target) {
    layout.outputBuses.getReference(0) = target;
    changed = true;
  }

  if (changed && !processor.setBusesLayout(layout)) {
    auto fallback = processor.getBusesLayout();

    if (!fallback.outputBuses.isEmpty()) {
      fallback.outputBuses.getReference(0) = target;
      processor.setBusesLayout(fallback);
    }
  }

  processor.prepareToPlay(sampleRate, maximumBlockSize);
}

void FxChain::process(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midi) noexcept {
  if (processors.empty()) return;

  const int numSamples = buffer.getNumSamples();
  const int bufferChannels = buffer.getNumChannels();

  if (numSamples <= 0 || bufferChannels <= 0) return;

  for (auto* processor : processors) {
    if (processor == nullptr || processor->isSuspended()) continue;

    const int processorChannels =
        juce::jmax(processor->getTotalNumInputChannels(),
                   processor->getTotalNumOutputChannels());

    if (processorChannels <= 0 || processorChannels > bufferChannels) continue;

    if (processorChannels == bufferChannels) {
      processor->processBlock(buffer, midi);
    } else {
      juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(),
                                    processorChannels, numSamples);
      processor->processBlock(view, midi);
    }
  }
}

}  // namespace AudioEngine
