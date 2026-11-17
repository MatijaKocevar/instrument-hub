#include "MeterBridge.h"

#include <algorithm>
#include <cmath>

namespace ihub::session {

namespace {
constexpr float meterDecay = 0.85f;
}

void MeterBridge::prepare(int slotCount) {
  const int slots = std::max(1, slotCount);

  if (slotMeterPeaks == nullptr || meterSlotCount != slots) {
    slotMeterPeaks =
        std::make_unique<std::atomic<float>[]>(static_cast<std::size_t>(slots));
    meterSlotCount = slots;
  }

  reset();
}

void MeterBridge::reset() noexcept {
  for (int slot = 0; slot < meterSlotCount; ++slot)
    slotMeterPeaks[static_cast<std::size_t>(slot)].store(
        0.0f, std::memory_order_relaxed);

  masterMeterPeak.store(0.0f, std::memory_order_relaxed);
}

void MeterBridge::update(const AudioEngine::AudioEngine& engine,
                         const AudioEngine::SlotSwitcher& switcher,
                         const juce::AudioBuffer<float>& master,
                         int numSamples) noexcept {
  if (slotMeterPeaks == nullptr) return;

  const int slots = std::min(meterSlotCount, engine.getNumSlots());

  for (int index = 0; index < slots; ++index) {
    float peak = 0.0f;

    const auto state = switcher.getSlotState(index);
    const float gain = switcher.getSlotGain(index);

    if (state != AudioEngine::SlotSwitcher::State::idle && gain > 0.0f) {
      const auto& rendered = engine.getSlot(index).getOutputBuffer();
      const int channels = rendered.getNumChannels();
      const int frames = std::min(numSamples, rendered.getNumSamples());

      for (int channel = 0; channel < channels; ++channel) {
        const auto* samples = rendered.getReadPointer(channel);

        for (int frame = 0; frame < frames; ++frame)
          peak = std::max(peak, std::abs(samples[frame]));
      }

      peak *= gain;
    }

    auto& meter = slotMeterPeaks[static_cast<std::size_t>(index)];
    const float decayed = meter.load(std::memory_order_relaxed) * meterDecay;
    meter.store(std::max(peak, decayed), std::memory_order_relaxed);
  }

  float masterPeak = 0.0f;
  const int channels = master.getNumChannels();
  const int frames = std::min(numSamples, master.getNumSamples());

  for (int channel = 0; channel < channels; ++channel) {
    const auto* samples = master.getReadPointer(channel);

    for (int frame = 0; frame < frames; ++frame)
      masterPeak = std::max(masterPeak, std::abs(samples[frame]));
  }

  const float decayed =
      masterMeterPeak.load(std::memory_order_relaxed) * meterDecay;
  masterMeterPeak.store(std::max(masterPeak, decayed),
                        std::memory_order_relaxed);
}

float MeterBridge::getSlotPeak(int slot) const noexcept {
  if (slotMeterPeaks == nullptr || slot < 0 || slot >= meterSlotCount)
    return 0.0f;

  return slotMeterPeaks[static_cast<std::size_t>(slot)].load(
      std::memory_order_relaxed);
}

float MeterBridge::getMasterPeak() const noexcept {
  return masterMeterPeak.load(std::memory_order_relaxed);
}

}  // namespace ihub::session
