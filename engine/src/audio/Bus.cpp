#include "Bus.h"

#include <algorithm>
#include <cmath>

namespace AudioEngine {

void Bus::prepare(double newSampleRate, int newMaximumBlockSize,
                  int newNumChannels) {
  jassert(newSampleRate > 0.0);
  jassert(newMaximumBlockSize > 0);
  jassert(newNumChannels > 0);

  sampleRate = newSampleRate;
  maximumBlockSize = juce::jmax(1, newMaximumBlockSize);
  numChannels = juce::jmax(1, newNumChannels);

  mixBuffer.setSize(numChannels, maximumBlockSize, false, true, true);
  mixBuffer.clear();

  limiter.prepare(sampleRate);
}

void Bus::releaseResources() {
  mixBuffer.setSize(0, 0);
  limiter.reset();
}

void Bus::reset() noexcept {
  mixBuffer.clear();
  limiter.reset();
}

void Bus::beginBlock(int numSamples) noexcept {
  const int samples = juce::jmin(numSamples, mixBuffer.getNumSamples());

  if (samples > 0) mixBuffer.clear(0, samples);
}

void Bus::add(const juce::AudioBuffer<float>& source, int numSamples) noexcept {
  const int samples =
      juce::jmin(numSamples, mixBuffer.getNumSamples(), source.getNumSamples());
  const int channels = juce::jmin(numChannels, source.getNumChannels());

  if (samples <= 0) return;

  for (int channel = 0; channel < channels; ++channel)
    mixBuffer.addFrom(channel, 0, source, channel, 0, samples);
}

void Bus::finish(juce::AudioBuffer<float>& destination,
                 int numSamples) noexcept {
  const int samples = juce::jmin(numSamples, mixBuffer.getNumSamples());

  if (samples <= 0) {
    destination.clear();
    return;
  }

  limiter.process(mixBuffer, samples);

  const int destinationChannels = destination.getNumChannels();
  const int copyChannels = juce::jmin(numChannels, destinationChannels);

  for (int channel = 0; channel < copyChannels; ++channel)
    destination.copyFrom(channel, 0, mixBuffer, channel, 0, samples);

  for (int channel = copyChannels; channel < destinationChannels; ++channel)
    destination.clear(channel, 0, destination.getNumSamples());

  for (int channel = 0; channel < copyChannels; ++channel)
    if (destination.getNumSamples() > samples)
      destination.clear(channel, samples,
                        destination.getNumSamples() - samples);
}

}  // namespace AudioEngine
