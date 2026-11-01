#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace ihub::dsp
{

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

struct DelayTransport
{
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;

    bool operator== (const DelayTransport&) const = default;
};

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

    static int delaySamplesForDivision (DelayDivision division,
                                        double bpm,
                                        double sampleRate) noexcept;

    static int delaySamplesForMs (float milliseconds, double sampleRate) noexcept;

    Delay();
    ~Delay() override = default;

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

    void setSettings (const DelaySettings& settings);
    DelaySettings getSettings() const noexcept;

    void setTempoSync (bool shouldSync);
    void setDivision (DelayDivision division);
    void setTimeMs (float milliseconds);
    void setFeedback (float feedback);
    void setMix (float mix);
    void setDampingHz (float dampingHz);

    void setTransport (const DelayTransport& transport);
    DelayTransport getTransport() const noexcept;

    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    int getDelaySamples() const noexcept;

    double getDelaySeconds() const noexcept;

    double getDelayInPpq() const noexcept;

    double getNextRepeatPpq() const noexcept;

    bool isPrepared() const noexcept { return prepared; }

private:

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

    juce::AudioBuffer<float> delayBuffer;
    int writeIndex = 0;
    int activeDelaySamples = 0;
    int previousDelaySamples = 0;
    int crossfadeRemaining = 0;
    int crossfadeLength = 0;
    std::vector<float> dampingState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Delay)
};

}
