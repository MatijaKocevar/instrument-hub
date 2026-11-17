#pragma once

#include "Slot.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <memory>
#include <vector>

namespace AudioEngine {

class SlotSwitcher {
public:
    enum class State { idle, warming, active, fading };

    static const char* toString(State state) noexcept;

    static constexpr float defaultFadeMilliseconds = 3.0f;

    explicit SlotSwitcher(float fadeMilliseconds = defaultFadeMilliseconds) noexcept;
    ~SlotSwitcher() = default;

    void setSlots(const std::vector<Slot*>& newSlots);

    void prepare(double sampleRate, int maximumBlockSize, int numChannels);

    void reset() noexcept;

    bool isPrepared() const noexcept { return prepared; }

    int getNumSlots() const noexcept { return numSlots; }
    Slot* getSlot(int slotIndex) const noexcept;

    void armSlot(int slotIndex) noexcept;

    void disarm() noexcept;

    int getArmedSlot() const noexcept;

    State getSlotState(int slotIndex) const noexcept;

    float getSlotGain(int slotIndex) const noexcept;

    bool process(int numSamples, juce::MidiBuffer& midi) noexcept;

    const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return mixBuffer; }

private:
    struct RuntimeState {
        State state = State::idle;
        float gain = 0.0f;
        float startGain = 0.0f;
        float targetGain = 0.0f;
    };

    void beginTransition(int slotIndex) noexcept;
    float gainAt(const RuntimeState& state, float phase) const noexcept;
    void publishStates() noexcept;

    std::vector<Slot*> slots;
    std::vector<RuntimeState> runtime;

    std::unique_ptr<std::atomic<int>[]> publishedStates;
    std::unique_ptr<std::atomic<float>[]> publishedGains;

    juce::AudioBuffer<float> mixBuffer;

    double sampleRate = 0.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
    int numSlots = 0;

    const float fadeMilliseconds;
    float fadeIncrement = 1.0f;
    bool prepared = false;

    std::atomic<int> pendingSlot{-1};

    int consumedSlot = -1;
    int incomingSlot = -1;
    float fadePosition = 0.0f;
    bool crossfading = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotSwitcher)
};

}  // namespace AudioEngine
