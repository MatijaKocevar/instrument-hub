#pragma once

#include "audio/AudioEngine.h"
#include "audio/SlotSwitcher.h"

#include <atomic>
#include <memory>

namespace ihub::session {

class MeterBridge {
public:
    void prepare(int slotCount);

    void reset() noexcept;

    void update(const AudioEngine::AudioEngine& engine, const AudioEngine::SlotSwitcher& switcher,
                const juce::AudioBuffer<float>& master, int numSamples) noexcept;

    float getSlotPeak(int slot) const noexcept;

    float getMasterPeak() const noexcept;

    int getSlotCount() const noexcept { return meterSlotCount; }

private:
    std::unique_ptr<std::atomic<float>[]> slotMeterPeaks;
    std::atomic<float> masterMeterPeak{0.0f};
    int meterSlotCount = 0;
};

}  // namespace ihub::session
