#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ihub::dsp
{

struct ReverbSettings
{
    float roomSize = 0.6f;
    float damping = 0.5f;
    float width = 1.0f;
    float mix = 0.25f;
    float preDelayMs = 0.0f;

    bool operator== (const ReverbSettings&) const = default;
};

class Reverb final : public juce::AudioProcessor
{
public:
    static constexpr float minRoomSize = 0.0f;
    static constexpr float maxRoomSize = 1.0f;
    static constexpr float minDamping = 0.0f;
    static constexpr float maxDamping = 1.0f;
    static constexpr float minWidth = 0.0f;
    static constexpr float maxWidth = 1.0f;
    static constexpr float minPreDelayMs = 0.0f;
    static constexpr float maxPreDelayMs = 200.0f;

    static constexpr int numCombs = 8;
    static constexpr int numAllpasses = 4;

    static constexpr float combFeedbackOffset = 0.7f;
    static constexpr float combFeedbackScale = 0.28f;

    static constexpr double maxTailSeconds = 20.0;

    Reverb();
    ~Reverb() override = default;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool hasEditor() const override                 { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    const juce::String getName() const override     { return "IHub Reverb"; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override    { return maxTailSeconds; }

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void setSettings (const ReverbSettings& settings);
    ReverbSettings getSettings() const noexcept;

    void setRoomSize (float roomSize);
    void setDamping (float damping);
    void setWidth (float width);
    void setMix (float mix);
    void setPreDelayMs (float milliseconds);

    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    bool isPrepared() const noexcept { return prepared; }

private:

    struct Comb
    {
        std::vector<float> buffer;
        int index = 0;
        float filterStore = 0.0f;

        void prepare (int size);
        void reset() noexcept;

        float process (float input, float feedback, float damp1, float damp2) noexcept;
    };

    struct Allpass
    {
        std::vector<float> buffer;
        int index = 0;

        void prepare (int size);
        void reset() noexcept;

        float process (float input, float feedback) noexcept;
    };

    struct Channel
    {
        std::array<Comb, numCombs> combs;
        std::array<Allpass, numAllpasses> allpasses;
        std::vector<float> preDelayBuffer;
        int preDelayIndex = 0;

        void prepare (int sampleRate, int preDelaySize, bool useStereoSpread);
        void reset() noexcept;

        float process (float input, int preDelaySamples,
                       float feedback, float damp1, float damp2) noexcept;
    };

    struct Published
    {
        float combFeedback = 0.868f;
        float damp1 = 0.2f;
        float damp2 = 0.8f;
        float wet1 = 0.25f;
        float wet2 = 0.0f;
        float dryGain = 0.75f;
        int preDelaySamples = 0;
    };

    struct AtomicPublished
    {
        std::atomic<float> combFeedback { 0.868f };
        std::atomic<float> damp1 { 0.2f };
        std::atomic<float> damp2 { 0.8f };
        std::atomic<float> wet1 { 0.25f };
        std::atomic<float> wet2 { 0.0f };
        std::atomic<float> dryGain { 0.75f };
        std::atomic<int> preDelaySamples { 0 };

        void store (const Published& p) noexcept
        {
            combFeedback.store (p.combFeedback, std::memory_order_relaxed);
            damp1.store (p.damp1, std::memory_order_relaxed);
            damp2.store (p.damp2, std::memory_order_relaxed);
            wet1.store (p.wet1, std::memory_order_relaxed);
            wet2.store (p.wet2, std::memory_order_relaxed);
            dryGain.store (p.dryGain, std::memory_order_relaxed);
            preDelaySamples.store (p.preDelaySamples, std::memory_order_relaxed);
        }

        Published load() const noexcept
        {
            Published p;
            p.combFeedback = combFeedback.load (std::memory_order_relaxed);
            p.damp1 = damp1.load (std::memory_order_relaxed);
            p.damp2 = damp2.load (std::memory_order_relaxed);
            p.wet1 = wet1.load (std::memory_order_relaxed);
            p.wet2 = wet2.load (std::memory_order_relaxed);
            p.dryGain = dryGain.load (std::memory_order_relaxed);
            p.preDelaySamples = preDelaySamples.load (std::memory_order_relaxed);
            return p;
        }
    };

    struct Snapshot
    {
        AtomicPublished values;
        std::atomic<std::uint32_t> version { 0 };
    };

    static ReverbSettings clampSettings (const ReverbSettings& settings) noexcept;

    Published computePublished() const noexcept;
    void publishSettings() noexcept;
    bool readSnapshot (Published& destination) const noexcept;

    ReverbSettings settings;
    std::atomic<bool> enabled { true };

    double currentSampleRate = 48000.0;
    bool prepared = false;

    Snapshot snapshot;
    std::uint32_t appliedVersion = 0;
    Published applied;

    std::vector<Channel> channels;
    int maxPreDelaySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Reverb)
};

}
