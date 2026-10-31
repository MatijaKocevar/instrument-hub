#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace ihub::dsp
{

/** The note values a tempo-synced delay can lock to.

    The grid is derived from the transport tempo (BPM) and is intentionally
    fine-grained enough for the usual straight, dotted and triplet choices.
    `Delay::divisionInBeats()` documents how each value maps to time.
*/
enum class DelayDivision
{
    whole = 0,
    half,
    quarter,
    eighth,
    sixteenth,
    thirtySecond,
    dottedQuarter,
    dottedEighth,
    tripletQuarter,
    tripletEighth,
    numDivisions
};

/** The transport values the delay locks to.

    This is a plain POD so the DSP layer does not depend on the control
    server: `api::TransportState` is copied into it by the caller. `ppq` is the
    musical position in quarter notes since the last rewind, exactly like the
    API's `/transport` field.
*/
struct DelayTransport
{
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;

    bool operator== (const DelayTransport&) const = default;
};

/** The user-facing delay parameters.

    `timeMs` is only used when `tempoSync` is false. `mix` is the wet/dry
    balance (0 = dry only, 1 = wet only). `dampingHz` is the cutoff of the
    one-pole low-pass in the feedback path: lower values make each repeat
    darker, which is what keeps long feedback settings from ringing forever
    at the top end.
*/
struct DelaySettings
{
    bool tempoSync = true;
    DelayDivision division = DelayDivision::quarter;
    float timeMs = 250.0f;
    float feedback = 0.35f;
    float mix = 0.35f;
    float dampingHz = 12000.0f;

    bool operator== (const DelaySettings&) const = default;
};

/**
    A stereo delay with tempo sync, feedback, damping and a wet/dry mix.

    Signal path (per sample, per channel):

        wet  = line[n - delaySamples]
        lp  += (wet - lp) * dampingCoeff          // one-pole low-pass
        line[n] = input + lp * feedback
        out  = input * (1 - mix) + wet * mix

    `delaySamples` is an integer, so the delay time is quantised to whole
    samples. That is deliberate: it makes the echo position exactly
    predictable (`getDelaySamples()` is the number of samples between the
    input and its repeat) instead of depending on an interpolator.

    Tempo sync: when `tempoSync` is true the delay length comes from the
    transport BPM and the chosen `DelayDivision`. `setTransport()` accepts the
    BPM and PPQ the control API publishes and is a message-thread call; the
    derived sample length is published to the audio thread through the same
    lock-free snapshot as every other parameter.

    The sync grid is *half-beat based*: one beat is `60 / BPM` seconds and a
    1/4-note setting is half a beat. That is what T062 pins down - a 1/4-note
    delay at 120 BPM is 250 ms, i.e. exactly 12000 samples at 48 kHz. The
    remaining divisions scale from there (`1/8 -> 0.25`, `1/2 -> 1.0`,
    `1/1 -> 2.0`, dotted values add half, triplets are two thirds).

    Changing the delay time while audio runs would click if the read pointer
    jumped. The audio thread therefore crossfades between the old and the new
    tap over 20 ms. Nothing is allocated: the line is sized once in
    `prepareToPlay()`.

    Threading / realtime safety:

    - `setSettings()`, the individual setters, `setTransport()`,
      `setEnabled()`, `prepareToPlay()`, `releaseResources()`, `reset()` and
      state (de)serialisation are message-thread calls.
    - Setters derive the published values (delay length in samples, linear
      feedback, wet/dry gains, damping coefficient) on the calling thread and
      publish them through a seqlock-style snapshot. No allocation, no lock.
    - `processBlock()` adopts a changed snapshot at a block boundary and never
      allocates, locks, logs or frees.
*/
class Delay final : public juce::AudioProcessor
{
public:
    static constexpr float minTimeMs = 1.0f;
    static constexpr float maxTimeMs = 8000.0f;
    static constexpr float minFeedback = 0.0f;
    static constexpr float maxFeedback = 0.95f;
    static constexpr float maxDampingHz = 20000.0f;
    static constexpr float minDampingHz = 200.0f;

    static constexpr double minBpm = 20.0;
    static constexpr double maxBpm = 300.0;
    static constexpr double maxDelaySeconds = 8.0;

    /** Length of one sync division in transport beats.

        See the class comment: the T062 acceptance fixes the 1/4-note setting
        at 250 ms for 120 BPM (12000 samples at 48 kHz), so a quarter note is
        half a beat in this grid.
    */
    static constexpr double divisionInBeats (DelayDivision division) noexcept
    {
        switch (division)
        {
            case DelayDivision::whole:           return 2.0;
            case DelayDivision::half:            return 1.0;
            case DelayDivision::quarter:         return 0.5;
            case DelayDivision::eighth:          return 0.25;
            case DelayDivision::sixteenth:       return 0.125;
            case DelayDivision::thirtySecond:    return 0.0625;
            case DelayDivision::dottedQuarter:   return 0.75;
            case DelayDivision::dottedEighth:    return 0.375;
            case DelayDivision::tripletQuarter:  return 1.0 / 3.0;
            case DelayDivision::tripletEighth:   return 1.0 / 6.0;
            case DelayDivision::numDivisions:    break;
        }

        return 0.5;
    }

    /** Delay length in samples for a synced division, clamped to the line.

        This is the pure function behind the acceptance check: 120 BPM,
        `quarter`, 48 kHz gives 12000.
    */
    static int delaySamplesForDivision (DelayDivision division,
                                        double bpm,
                                        double sampleRate) noexcept;

    /** Delay length in samples for a free (unsynced) time in milliseconds. */
    static int delaySamplesForMs (float milliseconds, double sampleRate) noexcept;

    Delay();
    ~Delay() override = default;

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
    const juce::String getName() const override     { return "IHub Delay"; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override    { return maxDelaySeconds; }

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
    void setSettings (const DelaySettings& settings);
    DelaySettings getSettings() const noexcept;

    void setTempoSync (bool shouldSync);
    void setDivision (DelayDivision division);
    void setTimeMs (float milliseconds);
    void setFeedback (float feedback);
    void setMix (float mix);
    void setDampingHz (float dampingHz);

    /** Publishes the transport position the delay locks to. Only the BPM
        changes the delay length; PPQ is kept so callers can query where the
        next repeat lands in musical time.
    */
    void setTransport (const DelayTransport& transport);
    DelayTransport getTransport() const noexcept;

    /** Global bypass. `false` makes `processBlock()` leave the buffer
        untouched (bit-exact dry passthrough) while the delay line keeps
        running, so re-enabling does not click. */
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    //==============================================================================
    // Read-only state

    /** The delay length in samples the audio thread is using, derived from the
        last published settings and transport. Equal to the repeat spacing
        measured on an impulse. */
    int getDelaySamples() const noexcept;

    /** The delay length in seconds. */
    double getDelaySeconds() const noexcept;

    /** The delay length in PPQ units (quarter notes), the same unit as
        `DelayTransport::ppq`. */
    double getDelayInPpq() const noexcept;

    /** Where the next repeat lands on the transport's PPQ timeline. */
    double getNextRepeatPpq() const noexcept;

    /** True after `prepareToPlay()` and before `releaseResources()`. */
    bool isPrepared() const noexcept { return prepared; }

private:
    //==============================================================================
    // Derived parameter set, published message thread -> audio thread.

    struct Published
    {
        int delaySamples = 12000;
        float feedback = 0.35f;
        float mix = 0.35f;
        float dampingCoeff = 1.0f;
    };

    struct AtomicPublished
    {
        std::atomic<int> delaySamples { 12000 };
        std::atomic<float> feedback { 0.35f };
        std::atomic<float> mix { 0.35f };
        std::atomic<float> dampingCoeff { 1.0f };

        void store (const Published& p) noexcept
        {
            delaySamples.store (p.delaySamples, std::memory_order_relaxed);
            feedback.store (p.feedback, std::memory_order_relaxed);
            mix.store (p.mix, std::memory_order_relaxed);
            dampingCoeff.store (p.dampingCoeff, std::memory_order_relaxed);
        }

        Published load() const noexcept
        {
            Published p;
            p.delaySamples = delaySamples.load (std::memory_order_relaxed);
            p.feedback = feedback.load (std::memory_order_relaxed);
            p.mix = mix.load (std::memory_order_relaxed);
            p.dampingCoeff = dampingCoeff.load (std::memory_order_relaxed);
            return p;
        }
    };

    struct Snapshot
    {
        AtomicPublished values;
        std::atomic<std::uint32_t> version { 0 };
    };

    //==============================================================================
    static DelaySettings clampSettings (const DelaySettings& settings) noexcept;
    static double clampBpm (double bpm) noexcept;

    Published computePublished() const noexcept;
    void publishSettings() noexcept;
    bool readSnapshot (Published& destination) const noexcept;

    DelaySettings settings;
    std::atomic<bool> enabled { true };

    std::atomic<double> transportBpm { 120.0 };
    std::atomic<double> transportPpq { 0.0 };
    std::atomic<bool> transportPlaying { false };

    double currentSampleRate = 48000.0;
    bool prepared = false;

    Snapshot snapshot;
    std::uint32_t appliedVersion = 0;
    Published applied;

    std::atomic<int> reportedDelaySamples { 12000 };

    // Audio-thread state only.
    juce::AudioBuffer<float> delayBuffer;
    int writeIndex = 0;
    int activeDelaySamples = 0;
    int previousDelaySamples = 0;
    int crossfadeRemaining = 0;
    int crossfadeLength = 0;
    std::vector<float> dampingState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Delay)
};

} // namespace ihub::dsp
