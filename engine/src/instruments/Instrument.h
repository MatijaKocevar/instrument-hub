#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub {

class Instrument {
 public:
  enum class Kind { Plugin, AudioInput, Sampler };

  virtual ~Instrument() = default;

  virtual void prepare(double sampleRate, int maxBlock) = 0;

  virtual void noteOn(int ch, int note, float vel) = 0;

  virtual void noteOff(int ch, int note) = 0;

  virtual void process(juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midi) = 0;

  virtual int latencySamples() const = 0;

  virtual Kind kind() const = 0;
};

const char* toString(Instrument::Kind kind) noexcept;

}  // namespace ihub
