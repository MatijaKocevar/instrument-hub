#pragma once

#include "FxChain.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace AudioEngine {

class SlotSource {
 public:
  virtual ~SlotSource() = default;

  virtual void prepare(double sampleRate, int maximumBlockSize) = 0;

  virtual void process(juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midi) = 0;

  virtual int latencySamples() const noexcept { return 0; }

  virtual bool acceptsDeviceInput() const noexcept { return false; }

  virtual void setInputBlock(const float* const* channelData,
                             int numChannels) noexcept {
    juce::ignoreUnused(channelData, numChannels);
  }

  virtual bool bypassesFxChain() const noexcept { return false; }
};

enum class SlotState { idle, fadingIn, active, fadingOut };

const char* toString(SlotState state) noexcept;

class Slot {
 public:
  static constexpr float defaultFadeMilliseconds = 3.0f;

  explicit Slot(int index, float fadeMilliseconds = defaultFadeMilliseconds);
  ~Slot() = default;

  void prepare(double sampleRate, int maximumBlockSize, int numChannels);

  void reset() noexcept;

  void setSource(SlotSource* newSource) noexcept;
  SlotSource* getSource() const noexcept { return source; }

  FxChain& getFxChain() noexcept { return fxChain; }
  const FxChain& getFxChain() const noexcept { return fxChain; }

  void setFxChainEditing(bool editing) noexcept {
    fxChainEditing.store(editing, std::memory_order_release);
  }

  bool isFxChainEditing() const noexcept {
    return fxChainEditing.load(std::memory_order_acquire);
  }

  void activate() noexcept;

  void deactivate() noexcept;

  bool isActivated() const noexcept;

  void setGain(float newGain) noexcept;
  float getGain() const noexcept;

  bool process(int numSamples, juce::MidiBuffer& midi) noexcept;

  void feedDeviceInput(const float* const* channelData,
                       int numChannels) noexcept;

  bool processUnfaded(int numSamples, juce::MidiBuffer& midi) noexcept;

  const juce::AudioBuffer<float>& getOutputBuffer() const noexcept {
    return view;
  }

  SlotState getState() const noexcept;
  bool isProcessing() const noexcept;

  int getIndex() const noexcept { return index; }

  int latencySamples() const noexcept;

  double latencyMilliseconds() const noexcept;

 private:
  bool renderSource(int numSamples, juce::MidiBuffer& midi) noexcept;
  void applyGains(int numSamples, bool applyFade) noexcept;

  const int index;
  const float fadeMilliseconds;

  juce::AudioBuffer<float> storage;
  juce::AudioBuffer<float> view;
  SlotSource* source = nullptr;
  FxChain fxChain;

  double sampleRate = 0.0;
  int maximumBlockSize = 0;
  int numChannels = 0;
  bool prepared = false;

  std::atomic<bool> activated{false};
  std::atomic<SlotState> publishedState{SlotState::idle};
  std::atomic<float> gainTarget{1.0f};
  std::atomic<bool> fxChainEditing{false};

  SlotState state = SlotState::idle;
  float fadeGain = 0.0f;
  float fadeStep = 0.0f;
  float appliedGain = 1.0f;
  float gainStep = 0.0f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Slot)
};

}  // namespace AudioEngine
