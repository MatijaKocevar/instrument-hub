#include "FakeToneInstrument.h"

#include <cmath>
#include <cstdlib>

namespace ihub::plugins::sandbox_client {
FakeToneInstrument::FakeToneInstrument(double sampleRateToUse, double frequency,
                                       int abortAfter)
    : increment(juce::MathConstants<double>::twoPi * frequency /
                sampleRateToUse),
      abortAfterBlocks(abortAfter) {}

void FakeToneInstrument::process(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer& midi) {
  juce::ignoreUnused(midi);

  if (abortAfterBlocks >= 0 && blocksRendered >= abortAfterBlocks) std::abort();

  const auto numSamples = buffer.getNumSamples();
  const auto amplitude = 0.25f;

  for (int sample = 0; sample < numSamples; ++sample) {
    const auto value = amplitude * static_cast<float>(std::sin(phase));

    phase += increment;

    if (phase >= juce::MathConstants<double>::twoPi)
      phase -= juce::MathConstants<double>::twoPi;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
      buffer.setSample(channel, sample, value);
  }

  ++blocksRendered;
}

}  // namespace ihub::plugins::sandbox_client
