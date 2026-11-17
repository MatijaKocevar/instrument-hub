#include "MasterLimiter.h"

#include <cmath>

namespace AudioEngine {

void MasterLimiter::prepare(double sampleRate, float thresholdDb,
                            float releaseMilliseconds) noexcept {
  gain = 1.0f;
  setThresholdDb(thresholdDb);

  const float releaseSamples =
      juce::jmax(1.0f, releaseMilliseconds * 0.001f *
                           static_cast<float>(juce::jmax(1.0, sampleRate)));

  releaseCoefficient = 1.0f - std::exp(-1.0f / releaseSamples);
}

void MasterLimiter::reset() noexcept { gain = 1.0f; }

void MasterLimiter::setThresholdDb(float thresholdDb) noexcept {
  thresholdInDb = thresholdDb;
  threshold = juce::Decibels::decibelsToGain(thresholdDb, -100.0f);

  if (gain > 1.0f) gain = 1.0f;
}

void MasterLimiter::process(juce::AudioBuffer<float>& buffer,
                            int numSamples) noexcept {
  const int samples = juce::jmin(numSamples, buffer.getNumSamples());
  const int channels = buffer.getNumChannels();

  for (int i = 0; i < samples; ++i) {
    float peak = 0.0f;

    for (int channel = 0; channel < channels; ++channel)
      peak = juce::jmax(peak, std::abs(buffer.getReadPointer(channel)[i]));

    if (peak > threshold) {
      const float requiredGain = threshold / peak;

      if (requiredGain < gain) gain = requiredGain;
    } else if (gain < 1.0f) {
      gain += releaseCoefficient * (1.0f - gain);

      if (gain > 1.0f || 1.0f - gain < 1.0e-6f) gain = 1.0f;
    }

    if (gain < 1.0f) {
      for (int channel = 0; channel < channels; ++channel)
        buffer.getWritePointer(channel)[i] *= gain;
    }
  }
}

float MasterLimiter::getGainReductionDb() const noexcept {
  if (gain <= 0.0f) return -100.0f;

  return 20.0f * std::log10(gain);
}

}  // namespace AudioEngine
