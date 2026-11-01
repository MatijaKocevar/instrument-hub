#pragma once

#include "audio/AudioEngine.h"
#include "audio/SlotSwitcher.h"

#include <atomic>
#include <memory>

namespace ihub::session
{

/**
    Publishes per-slot and master peak meters.

    The audio path writes one relaxed atomic per slot after each block; the UI
    and the HTTP API read them without a lock. Storage is allocated once in
    `prepare()` (never in `update()`), so the 64-slot session stays inside the
    audio-thread contract.
*/
class MeterBridge
{
public:
    /** Allocates or reuses the meter array for `slotCount` slots. */
    void prepare (int slotCount);

    /** Clears every meter (graph reset / device restart). */
    void reset() noexcept;

    /** Audio-thread call after a block has been mixed into `master`. */
    void update (const AudioEngine::AudioEngine& engine,
                 const AudioEngine::SlotSwitcher& switcher,
                 const juce::AudioBuffer<float>& master,
                 int numSamples) noexcept;

    /** Most recent crossfade-weighted peak of one slot, 0 when silent. */
    float getSlotPeak (int slot) const noexcept;

    /** Most recent peak of the mixed master bus. */
    float getMasterPeak() const noexcept;

    int getSlotCount() const noexcept { return meterSlotCount; }

private:
    std::unique_ptr<std::atomic<float>[]> slotMeterPeaks;
    std::atomic<float> masterMeterPeak { 0.0f };
    int meterSlotCount = 0;
};

} // namespace ihub::session
