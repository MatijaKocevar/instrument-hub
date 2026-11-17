#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>

namespace ihub::session {

class MidiBridge {
 public:
  static constexpr int queueCapacity = 1024;

  MidiBridge() = default;

  void prepare();

  void reset() noexcept;

  bool push(const juce::MidiMessage& message) noexcept;

  void drain(juce::MidiBuffer& destination) noexcept;

  int getQueuedCount() const noexcept { return fifo.getNumReady(); }

  juce::MidiBuffer& block() noexcept { return blockMidi; }

 private:
  struct QueuedMidi {
    std::array<std::uint8_t, 3> data{};
    std::uint8_t size = 0;
  };

  juce::AbstractFifo fifo{queueCapacity};
  std::array<QueuedMidi, queueCapacity> queue{};
  juce::MidiBuffer blockMidi;
};

}  // namespace ihub::session
