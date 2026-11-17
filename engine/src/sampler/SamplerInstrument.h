#pragma once

#include "../audio/Slot.h"
#include "../instruments/Instrument.h"

#include "KitLoader.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace ihub::sampler {

class SamplerInstrument final : public Instrument,
                                public AudioEngine::SlotSource {
 public:
  static constexpr int maxVoices = 64;

  static constexpr int maxPendingEvents = 512;

  enum class PadMode { oneShot, gate };

  static constexpr int noChokeGroup = 0;

  static constexpr int maximumChokeGroup = 255;

  static constexpr float defaultChokeFadeMilliseconds = 2.0f;
  static constexpr float maximumChokeFadeMilliseconds = 5.0f;
  static constexpr float defaultGateReleaseMilliseconds = 5.0f;

  SamplerInstrument();
  ~SamplerInstrument() override;

  void setKit(LoadedKit* kit) noexcept;

  const LoadedKit* getKit() const noexcept;

  void setPadMode(int note, PadMode mode) noexcept;
  PadMode getPadMode(int note) const noexcept;

  void setChokeGroup(int note, int group) noexcept;
  int getChokeGroup(int note) const noexcept;

  void setChokeFadeMilliseconds(float milliseconds) noexcept;
  float getChokeFadeMilliseconds() const noexcept;

  void setGateReleaseMilliseconds(float milliseconds) noexcept;
  float getGateReleaseMilliseconds() const noexcept;

  void reset() noexcept;

  void prepare(double sampleRate, int maxBlock) override;

  void noteOn(int ch, int note, float vel) override;

  void noteOff(int ch, int note) override;

  void allNotesOff(int ch = 1);

  void process(juce::AudioBuffer<float>& buffer,
               juce::MidiBuffer& midi) override;

  int latencySamples() const noexcept override { return 0; }

  Kind kind() const noexcept override { return Kind::Sampler; }

  int getVoiceCapacity() const noexcept { return maxVoices; }
  int getActiveVoiceCount() const noexcept;

  int getStolenVoiceCount() const noexcept;

  int getDroppedEventCount() const noexcept;

  int getStartedVoiceCount() const noexcept;

  int getPendingEventCount() const noexcept;

 private:
  struct PendingEvent {
    std::uint8_t bytes[3]{0, 0, 0};
    std::uint8_t numBytes = 0;
  };

  struct Voice {
    const SampleData* sample = nullptr;

    int note = 0;
    int channel = 0;
    int chokeGroup = noChokeGroup;

    double position = 0.0;
    double ratio = 1.0;

    float leftGain = 0.0f;
    float rightGain = 0.0f;

    float gain = 1.0f;

    float fadeStep = 0.0f;

    std::uint64_t serial = 0;

    PadMode mode = PadMode::oneShot;
    bool active = false;
  };

  static int clampNote(int note) noexcept;
  static int clampGroup(int group) noexcept;

  bool pushEvent(const std::uint8_t* bytes, int numBytes) noexcept;
  void drainQueuedEvents() noexcept;
  void applyQueuedEvent(const PendingEvent& event) noexcept;
  void applyMidiEvent(const juce::MidiMessageMetadata& metadata) noexcept;

  void startVoice(int channel, int note, int velocity) noexcept;
  void releaseVoices(int channel, int note) noexcept;
  void chokeVoices(int group, int excludeVoice) noexcept;
  void stopAllVoicesNow() noexcept;
  void allNotesOffNow() noexcept;

  int findFreeVoice() const noexcept;
  int findVoiceToSteal() const noexcept;
  float fadeStepForMilliseconds(float milliseconds) const noexcept;

  void renderVoices(juce::AudioBuffer<float>& buffer, int startSample,
                    int numSamples) noexcept;

  void setVoiceInactive(Voice& voice) noexcept;

  std::atomic<LoadedKit*> kit{nullptr};

  std::array<std::atomic<int>, 128> padModes{};
  std::array<std::atomic<int>, 128> chokeGroups{};

  std::atomic<float> chokeFadeMilliseconds{defaultChokeFadeMilliseconds};
  std::atomic<float> gateReleaseMilliseconds{defaultGateReleaseMilliseconds};

  double sampleRate = 48000.0;
  int maximumBlockSize = 0;

  std::array<Voice, maxVoices> voices{};
  std::uint64_t nextSerial = 1;

  std::atomic<int> activeVoiceCount{0};
  std::atomic<int> stolenVoiceCount{0};
  std::atomic<int> droppedEventCount{0};
  std::atomic<int> startedVoiceCount{0};

  std::array<PendingEvent, maxPendingEvents> pendingEvents{};
  std::atomic<std::uint32_t> pendingWriteIndex{0};
  std::atomic<std::uint32_t> pendingReadIndex{0};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerInstrument)
};

}  // namespace ihub::sampler
