#pragma once

#include "audio/Slot.h"

#include <atomic>

namespace ihub::session::detail {

class WarmPoolVoice final : public AudioEngine::SlotSource {
 public:
  void prepare(double, int) override {}

  void process(juce::AudioBuffer<float>& buffer,
               juce::MidiBuffer& midi) override;

  int latencySamples() const noexcept override {
    return latency.load(std::memory_order_relaxed);
  }

  bool acceptsDeviceInput() const noexcept override {
    return acceptsInput.load(std::memory_order_relaxed);
  }

  void setInputBlock(const float* const* channelData,
                     int numInputs) noexcept override;

  bool bypassesFxChain() const noexcept override {
    return bypassesChain.load(std::memory_order_relaxed);
  }

  void bind(AudioEngine::SlotSource* instance) noexcept;

  void clearIf(AudioEngine::SlotSource* instance) noexcept;

 private:
  std::atomic<AudioEngine::SlotSource*> delegate{nullptr};
  std::atomic<int> latency{0};
  std::atomic<bool> acceptsInput{false};
  std::atomic<bool> bypassesChain{false};
};

}  // namespace ihub::session::detail
