#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ihub::dsp
{

/** The three band shapes the built-in EQ understands.

    A "3-band parametric" EQ is a low shelf, a bell (peak) and a high shelf.
    The bell is the fully parametric band; the shelves use the same
    frequency/gain/Q controls, with Q acting as their slope.
*/
enum class EqBandType
{
    lowShelf = 0,
    peak,
    highShelf
};

/** One band's user-facing parameters. */
struct EqBandSettings
{
    EqBandType type = EqBandType::peak;
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.7071068f; // Butterworth-ish slope for the shelves
    bool enabled = true;

    bool operator== (const EqBandSettings&) const = default;
};

/**
    A 3-band parametric EQ built on `juce::dsp::IIR`.

    Signal path: low shelf -> peak -> high shelf, each band a second-order
    biquad per channel. A band with 0 dB gain (or `enabled == false`) has an
    exactly identity transfer function, so an untouched EQ is a bit-exact
    passthrough.

    Threading / realtime safety:

    - `setBand()`, `setAllBands()`, `setEnabled()`, `prepareToPlay()`,
      `releaseResources()`, `reset()` and state (de)serialisation are
      message-thread calls.
    - `setBand()` designs the new biquad coefficients and publishes them into a
      lock-free, seqlock-style snapshot. No allocation, no lock.
    - `processBlock()` notices a changed snapshot at a block boundary, copies
      the five normalised coefficients into the pre-allocated
      `juce::dsp::IIR::Coefficients` object (a plain memory copy) and filters.
      It never allocates, locks, logs, or frees.
    - `getMagnitudeDbAt()` reads the snapshot atomically and may be called
      while the audio thread runs (for example by the UI).

    The published coefficients are deliberately not crossfaded: a parameter
    change is applied between blocks, which leaves the IIR state intact and
    avoids resetting the filter. A UI that wants click-free ramping can send
    small steps; smoothing is outside this task.
*/
class Eq final : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 3;

    static constexpr float minFrequencyHz = 20.0f;
    static constexpr float maxFrequencyHz = 20000.0f;
    static constexpr float minQ = 0.1f;
    static constexpr float maxQ = 18.0f;
    static constexpr float maxGainDb = 24.0f;

    Eq();
    ~Eq() override = default;

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
    const juce::String getName() const override     { return "IHub EQ"; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override    { return 0.0; }

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameters (message thread)

    /** Sets one band. Out-of-range indices are ignored.

        Values are clamped to the ranges above and to just below Nyquist for the
        current sample rate. The new coefficients are published to the audio
        thread immediately and applied at its next block boundary.
    */
    void setBand (int index, const EqBandSettings& settings);

    /** Returns the stored settings; safe to call before `prepareToPlay()`. */
    EqBandSettings getBand (int index) const noexcept;

    /** Replaces all three bands with one publish. */
    void setAllBands (const std::array<EqBandSettings, numBands>& settings);

    std::array<EqBandSettings, numBands> getAllBands() const noexcept;

    /** Global bypass. `false` makes `processBlock()` leave the buffer untouched. */
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    /** True after `prepareToPlay()` and before `releaseResources()`. */
    bool isPrepared() const noexcept { return prepared; }

    /** Analytic magnitude (dB) of the currently published coefficients at
        `frequencyHz`, for a UI response curve or a quick check.

        Safe to call while the audio thread is running: only atomic state is
        read. Like every other parameter call, it assumes `prepareToPlay()` is
        not running concurrently.
    */
    double getMagnitudeDbAt (double frequencyHz) const noexcept;

private:
    //==============================================================================
    // Plain coefficient POD: biquad normalised so a0 == 1.

    struct BiquadCoefficients
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };

    //==============================================================================
    // Lock-free publication.
    //
    // The message thread is the only writer. It toggles `version` to an odd
    // value, writes the three coefficient sets as relaxed atomics, then stores
    // an even version with release semantics. The audio thread reads the
    // version, copies the fields, and re-reads the version; a mismatch means
    // the writer was mid-update, so it keeps its current coefficients for this
    // block and retries on the next one. No locks and no allocation either
    // side.

    struct AtomicBiquad
    {
        std::atomic<float> b0 { 1.0f };
        std::atomic<float> b1 { 0.0f };
        std::atomic<float> b2 { 0.0f };
        std::atomic<float> a1 { 0.0f };
        std::atomic<float> a2 { 0.0f };

        void store (const BiquadCoefficients& c) noexcept
        {
            b0.store (c.b0, std::memory_order_relaxed);
            b1.store (c.b1, std::memory_order_relaxed);
            b2.store (c.b2, std::memory_order_relaxed);
            a1.store (c.a1, std::memory_order_relaxed);
            a2.store (c.a2, std::memory_order_relaxed);
        }

        BiquadCoefficients load() const noexcept
        {
            return { b0.load (std::memory_order_relaxed),
                     b1.load (std::memory_order_relaxed),
                     b2.load (std::memory_order_relaxed),
                     a1.load (std::memory_order_relaxed),
                     a2.load (std::memory_order_relaxed) };
        }
    };

    struct Snapshot
    {
        std::array<AtomicBiquad, numBands> bands;
        std::atomic<std::uint32_t> version { 0 };
    };

    //==============================================================================
    static BiquadCoefficients designBand (const EqBandSettings& settings,
                                          double sampleRate) noexcept;

    static juce::dsp::IIR::Coefficients<float>::Ptr makeJuceCoefficients (
        const BiquadCoefficients& coefficients);

    void publishSettings() noexcept;
    bool readSnapshot (std::array<BiquadCoefficients, numBands>& destination) const noexcept;
    void applySnapshot (const std::array<BiquadCoefficients, numBands>& coefficients) noexcept;

    std::array<EqBandSettings, numBands> bandSettings;
    std::atomic<bool> enabled { true };

    double currentSampleRate = 48000.0;
    bool prepared = false;

    Snapshot snapshot;
    std::uint32_t appliedVersion = 0;

    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numBands> bandCoefficients;
    std::vector<std::array<juce::dsp::IIR::Filter<float>, numBands>> channelFilters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Eq)
};

} // namespace ihub::dsp
