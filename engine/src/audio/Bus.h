#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MasterLimiter.h"

namespace AudioEngine {

class Bus {
 public:
  Bus() = default;

  void prepare(double sampleRate, int maximumBlockSize, int numChannels);

  void releaseResources();

  void reset() noexcept;

  void beginBlock(int numSamples) noexcept;

  void add(const juce::AudioBuffer<float>& source, int numSamples) noexcept;

  void finish(juce::AudioBuffer<float>& destination, int numSamples) noexcept;

  int getNumChannels() const noexcept { return numChannels; }
  int getMaximumBlockSize() const noexcept { return maximumBlockSize; }

  MasterLimiter& getLimiter() noexcept { return limiter; }
  const MasterLimiter& getLimiter() const noexcept { return limiter; }

  const juce::AudioBuffer<float>& getMixBuffer() const noexcept {
    return mixBuffer;
  }

 private:
  juce::AudioBuffer<float> mixBuffer;
  MasterLimiter limiter;
  double sampleRate = 48000.0;
  int maximumBlockSize = 0;
  int numChannels = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Bus)
};

}  // namespace AudioEngine
