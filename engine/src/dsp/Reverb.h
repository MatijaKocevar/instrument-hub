#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ihub::dsp
{

/** The user-facing reverb parameters.

    `roomSize` and `damping` are normalised 0..1: room size changes the comb
    feedback (and therefore the decay time), damping controls the low-pass in
    each comb. `width` spreads the two output channels (0 = mono, 1 = wide).
    `mix` is the wet/dry balance. `preDelayMs` inserts a delay before the
    reverb network, which separates the dry attack from the tail.
*/
struct ReverbSettings
{
    float roomSize = 0.6f;
    float damping = 0.5f;
    float width = 1.0f;
    float mix = 0.25f;
    float preDelayMs = 0.0f;

    bool operator== (const ReverbSettings&) const = default;
};

/**
    A Freeverb-style algorithmic reverb: eight parallel damped comb filters
    per channel, feeding four series allpass filters.

    The topology is the classic Schroeder/Freeverb one:

        in -> pre-delay -> [8 parallel combs] -> [4 series allpasses] -> wet

    Each comb is a feedback comb with a one-pole low-pass in its loop, so the
    tail darkens as it decays. The right channel uses the same tunings plus a
    23-sample stereo spread, and the two wet channels are mixed with a width
    control. Tunings are the Freeverb tables scaled from 44.1 kHz to the
    current sample rate in `prepareToPlay()`.

    Threading / realtime safety:

    - `setSettings()`, the individual setters, `setEnabled()`,
      `prepareToPlay()`, `releaseResources()`, `reset()` and state
      (de)serialisation are message-thread calls.
    - All gain coefficients (comb feedback, damping, width, wet/dry) are
      derived on the calling thread and published through a lock-free,
      seqlock-style snapshot. No allocation, no lock.
    - `processBlock()` adopts a changed snapshot at a block boundary and never
      allocates, locks, logs or frees. Every buffer is sized in
      `prepareToPlay()`.
    - Bypass keeps the network running and outputs a bit-exact dry signal, so
      re-enabling mid-tail is click-free.
*/
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

    /** Freeverb's comb feedback range: feedback = offset + scale * roomSize. */
    static constexpr float combFeedbackOffset = 0.7f;
    static constexpr float combFeedbackScale = 0.28f;

    /** A generous hint for hosts that render a tail. At maximum room size the
        comb feedback is 0.98, which rings for well over ten seconds. */
    static constexpr double maxTailSeconds = 20.0;

    Reverb();
    ~Reverb() override = default;

    //==============================================================================
    // AudioProcessor

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

    //==============================================================================
    // Parameters (message thread)

    /** Replaces all settings with one publish. Values are clamped. */
    void setSettings (const ReverbSettings& settings);
    ReverbSettings getSettings() const noexcept;

    void setRoomSize (float roomSize);
    void setDamping (float damping);
    void setWidth (float width);
    void setMix (float mix);
    void setPreDelayMs (float milliseconds);

    /** Global bypass. `false` leaves the buffer untouched (bit-exact dry
        passthrough) while the tail keeps running. */
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    /** True after `prepareToPlay()` and before `releaseResources()`. */
    bool isPrepared() const noexcept { return prepared; }

private:
    //==============================================================================
    // Freeverb building blocks. All buffers are resized in prepareToPlay();
    // the process calls only touch memory that is already allocated.

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

    //==============================================================================
    // Derived parameter set, published message thread -> audio thread.

    struct Published
    {
        float combFeedback = 0.868f; // 0.7 + 0.28 * 0.6
        float damp1 = 0.2f;          // damping * 0.4
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

    //==============================================================================
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

} // namespace ihub::dsp
