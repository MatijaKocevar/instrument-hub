#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ihub::dsp {

enum class EqBandType { lowShelf = 0, peak, highShelf };

struct EqBandSettings {
    EqBandType type = EqBandType::peak;
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.7071068f;
    bool enabled = true;

    bool operator==(const EqBandSettings&) const = default;
};

class Eq final : public juce::AudioProcessor {
public:
    static constexpr int numBands = 3;

    static constexpr float minFrequencyHz = 20.0f;
    static constexpr float maxFrequencyHz = 20000.0f;
    static constexpr float minQ = 0.1f;
    static constexpr float maxQ = 18.0f;
    static constexpr float maxGainDb = 24.0f;

    Eq();
    ~Eq() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    const juce::String getName() const override { return "IHub EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void setBand(int index, const EqBandSettings& settings);

    EqBandSettings getBand(int index) const noexcept;

    void setAllBands(const std::array<EqBandSettings, numBands>& settings);

    std::array<EqBandSettings, numBands> getAllBands() const noexcept;

    void setEnabled(bool shouldBeEnabled);
    bool isEnabled() const noexcept;

    bool isPrepared() const noexcept { return prepared; }

    double getMagnitudeDbAt(double frequencyHz) const noexcept;

private:
    struct BiquadCoefficients {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };

    struct AtomicBiquad {
        std::atomic<float> b0{1.0f};
        std::atomic<float> b1{0.0f};
        std::atomic<float> b2{0.0f};
        std::atomic<float> a1{0.0f};
        std::atomic<float> a2{0.0f};

        void store(const BiquadCoefficients& c) noexcept {
            b0.store(c.b0, std::memory_order_relaxed);
            b1.store(c.b1, std::memory_order_relaxed);
            b2.store(c.b2, std::memory_order_relaxed);
            a1.store(c.a1, std::memory_order_relaxed);
            a2.store(c.a2, std::memory_order_relaxed);
        }

        BiquadCoefficients load() const noexcept {
            return {b0.load(std::memory_order_relaxed), b1.load(std::memory_order_relaxed),
                    b2.load(std::memory_order_relaxed), a1.load(std::memory_order_relaxed),
                    a2.load(std::memory_order_relaxed)};
        }
    };

    struct Snapshot {
        std::array<AtomicBiquad, numBands> bands;
        std::atomic<std::uint32_t> version{0};
    };

    static BiquadCoefficients designBand(const EqBandSettings& settings,
                                         double sampleRate) noexcept;

    static juce::dsp::IIR::Coefficients<float>::Ptr makeJuceCoefficients(
        const BiquadCoefficients& coefficients);

    void publishSettings() noexcept;
    bool readSnapshot(std::array<BiquadCoefficients, numBands>& destination) const noexcept;
    void applySnapshot(const std::array<BiquadCoefficients, numBands>& coefficients) noexcept;

    std::array<EqBandSettings, numBands> bandSettings;
    std::atomic<bool> enabled{true};

    double currentSampleRate = 48000.0;
    bool prepared = false;

    Snapshot snapshot;
    std::uint32_t appliedVersion = 0;

    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numBands> bandCoefficients;
    std::vector<std::array<juce::dsp::IIR::Filter<float>, numBands>> channelFilters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Eq)
};

}  // namespace ihub::dsp
