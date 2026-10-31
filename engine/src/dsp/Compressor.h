#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <cstdint>

namespace ihub::dsp
{

/** The five user-facing compressor parameters.

    `thresholdDb` is a peak level in dBFS: the detector follows the highest
    absolute sample value in the input, so a full-scale sine reads 0 dB.
    `ratio` is the usual "N:1" below-threshold slope inverse; 1 means no
    compression. `attackMs` / `releaseMs` are one-pole time constants for the
    gain movement. `makeupDb` is a static output gain, applied after the gain
    reduction and independent of it.
*/
struct CompressorSettings
{
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float makeupDb = 0.0f;

    bool operator== (const CompressorSettings&) const = default;
};

/**
    A feed-forward peak compressor with a fixed lookahead.

    Signal path (per sample, channels linked):

        peak  = max over channels of |input[n]|
        peak  = max (peak, peak * releaseDecay)     // peak-hold detector
        level = 20 * log10 (peak)
        target = max (0, (level - threshold) * (1 - 1/ratio))
        gr    += (target - gr) * (target > gr ? attackCoeff : releaseCoeff)
        out[n] = input[n - lookahead] * 10^(-gr/20) * makeup

    The detector reads the *incoming* sample while the audio comes from a
    delay line of exactly `lookaheadSamples` samples. Gain reduction therefore
    starts before the transient reaches the output. The delay is the entire
    reported latency, and `getLatencySamples()` is set to it in
    `prepareToPlay()` so plugin delay compensation lines the slot back up.

    Because the detector is a peak follower whose decay is the release time,
    the static curve holds for steady tones: a sine 6 dB over a 4:1 threshold
    settles at 4.5 dB of gain reduction, independent of the attack setting.
    There is no soft knee - the curve is piecewise linear, which is what makes
    the static behaviour exactly predictable.

    Bypass (`setEnabled (false)`) keeps the lookahead delay in the path and
    outputs the delayed input at unity gain. The reported latency stays valid,
    so a host does not have to re-run delay compensation while the user clicks
    the bypass button. The detector keeps running so that re-enabling is
    click-free.

    Threading / realtime safety:

    - `setSettings()`, the individual setters, `setEnabled()`,
      `setLookaheadMs()`, `prepareToPlay()`, `releaseResources()`, `reset()`
      and state (de)serialisation are message-thread calls.
    - The setters compute the derived values (slope, one-pole coefficients,
      peak decay, linear makeup gain) on the calling thread and publish them
      through a lock-free, seqlock-style snapshot. No allocation, no lock.
    - `processBlock()` picks up a changed snapshot at a block boundary and
      applies it for the rest of the block. It never allocates, locks, logs or
      frees. All buffers are sized in `prepareToPlay()`.
    - `getGainReductionDb()` reads an atomic written at the end of each block;
      safe to poll from a meter thread.
*/
class Compressor final : public juce::AudioProcessor
{
public:
    static constexpr float minThresholdDb = -60.0f;
    static constexpr float maxThresholdDb = 0.0f;
    static constexpr float minRatio = 1.0f;
    static constexpr float maxRatio = 20.0f;
    static constexpr float minAttackMs = 0.1f;
    static constexpr float maxAttackMs = 500.0f;
    static constexpr float minReleaseMs = 5.0f;
    static constexpr float maxReleaseMs = 5000.0f;
    static constexpr float minMakeupDb = -24.0f;
    static constexpr float maxMakeupDb = 24.0f;

    static constexpr float minLookaheadMs = 0.0f;
    static constexpr float maxLookaheadMs = 20.0f;
    static constexpr float defaultLookaheadMs = 5.0f;

    Compressor();
    ~Compressor() override = default;

    //==============================================================================
    // AudioProcessor

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool hasEditor() const override                  { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    const juce::String getName() const override      { return "IHub Compressor"; }
    bool acceptsMidi() const override                { return false; }
    bool producesMidi() const override               { return false; }
    double getTailLengthSeconds() const override     { return 0.0; }

    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameters (message thread)

    /** Replaces all five settings with one publish. Out-of-range values are
        clamped to the constants above. */
    void setSettings (const CompressorSettings& settings);

    /** Returns the stored settings; safe to call before `prepareToPlay()`. */
    CompressorSettings getSettings() const noexcept;

    void setThresholdDb (float value);
    void setRatio (float value);
    void setAttackMs (float value);
    void setReleaseMs (float value);
    void setMakeupDb (float value);

    /** Global bypass. `false` still delays by the reported lookahead but
        applies neither gain reduction nor makeup gain (bit-exact delayed
        passthrough). */
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    /** Lookahead in milliseconds. Takes effect at the next `prepareToPlay()`,
        because it resizes the delay line and changes the reported latency.
    */
    void setLookaheadMs (float milliseconds);
    float getLookaheadMs() const noexcept;

    /** The lookahead actually in use, in samples. Equals the reported
        latency (`getLatencySamples()`). */
    int getLookaheadSamples() const noexcept   { return lookaheadSamples; }

    /** Gain reduction in dB at the end of the last processed block, useful for
        a meter. 0 when bypassed or when nothing over threshold was detected. */
    float getGainReductionDb() const noexcept;

    /** True after `prepareToPlay()` and before `releaseResources()`. */
    bool isPrepared() const noexcept           { return prepared; }

private:
    //==============================================================================
    // Derived parameter set, published message thread -> audio thread.

    struct Published
    {
        float thresholdDb = -18.0f;
        float slope = 0.75f;        // 1 - 1/ratio
        float attackCoeff = 0.002f; // 1 - exp (-1 / tau)
        float releaseCoeff = 0.0002f;
        float peakDecay = 0.9998f;  // exp (-1 / tauRelease)
        float makeupGain = 1.0f;    // linear
    };

    struct AtomicPublished
    {
        std::atomic<float> thresholdDb { -18.0f };
        std::atomic<float> slope { 0.75f };
        std::atomic<float> attackCoeff { 0.002f };
        std::atomic<float> releaseCoeff { 0.0002f };
        std::atomic<float> peakDecay { 0.9998f };
        std::atomic<float> makeupGain { 1.0f };

        void store (const Published& p) noexcept
        {
            thresholdDb.store (p.thresholdDb, std::memory_order_relaxed);
            slope.store (p.slope, std::memory_order_relaxed);
            attackCoeff.store (p.attackCoeff, std::memory_order_relaxed);
            releaseCoeff.store (p.releaseCoeff, std::memory_order_relaxed);
            peakDecay.store (p.peakDecay, std::memory_order_relaxed);
            makeupGain.store (p.makeupGain, std::memory_order_relaxed);
        }

        Published load() const noexcept
        {
            Published p;
            p.thresholdDb = thresholdDb.load (std::memory_order_relaxed);
            p.slope = slope.load (std::memory_order_relaxed);
            p.attackCoeff = attackCoeff.load (std::memory_order_relaxed);
            p.releaseCoeff = releaseCoeff.load (std::memory_order_relaxed);
            p.peakDecay = peakDecay.load (std::memory_order_relaxed);
            p.makeupGain = makeupGain.load (std::memory_order_relaxed);
            return p;
        }
    };

    struct Snapshot
    {
        AtomicPublished values;
        std::atomic<std::uint32_t> version { 0 };
    };

    //==============================================================================
    static CompressorSettings clampSettings (const CompressorSettings& settings) noexcept;
    static float onePoleCoefficient (float timeMs, double sampleRate) noexcept;

    Published computePublished() const noexcept;
    void publishSettings() noexcept;
    bool readSnapshot (Published& destination) const noexcept;

    /** One detector + gain-shaper step; returns the linear gain to apply to
        the delayed sample. Always called, even while bypassed, so the state
        stays warm. */
    float advanceGain (float inputPeak, const Published& published) noexcept;

    CompressorSettings settings;
    std::atomic<bool> enabled { true };
    std::atomic<float> lookaheadMs { defaultLookaheadMs };

    double currentSampleRate = 48000.0;
    bool prepared = false;

    Snapshot snapshot;
    std::uint32_t appliedVersion = 0;
    Published applied;

    int lookaheadSamples = 0;
    int delayIndex = 0;
    juce::AudioBuffer<float> delayBuffer;

    // Audio-thread state only.
    float detectorPeak = 0.0f;
    float gainReductionDb = 0.0f;

    std::atomic<float> reportedGainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compressor)
};

} // namespace ihub::dsp
