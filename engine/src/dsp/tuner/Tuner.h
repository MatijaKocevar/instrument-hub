#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <array>
#include <atomic>

namespace ihub::dsp
{

    struct TunerReading
    {
        bool valid = false;
        float frequencyHz = 0.0f;
        int midiNote = -1;
        float cents = 0.0f;
        float confidence = 0.0f;
        float levelDb = -120.0f;
    };

    class Tuner final : public juce::AudioProcessor
    {
    public:
        static constexpr float defaultReferenceHz = 440.0f;
        static constexpr float minReferenceHz = 400.0f;
        static constexpr float maxReferenceHz = 480.0f;

        Tuner ();
        ~Tuner () override = default;

        void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
        void releaseResources () override;
        void reset () override;

        using juce::AudioProcessor::processBlock;
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

        bool hasEditor () const override
        {
            return false;
        }
        juce::AudioProcessorEditor* createEditor () override
        {
            return nullptr;
        }
        const juce::String getName () const override
        {
            return "IHub Tuner";
        }
        bool acceptsMidi () const override
        {
            return false;
        }
        bool producesMidi () const override
        {
            return false;
        }
        double getTailLengthSeconds () const override
        {
            return 0.0;
        }

        int getNumPrograms () override
        {
            return 1;
        }
        int getCurrentProgram () override
        {
            return 0;
        }
        void setCurrentProgram (int) override
        {
        }
        const juce::String getProgramName (int) override
        {
            return "Default";
        }
        void changeProgramName (int, const juce::String&) override
        {
        }

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        void setReferenceHz (float hz);
        float getReferenceHz () const noexcept;

        TunerReading getReading () const noexcept;

    private:
        static constexpr int windowSize = 1024;
        static constexpr int maximumLagSamples = 220;
        static constexpr int ringSize = 2048;
        static constexpr int analysisSize = windowSize + maximumLagSamples;
        static constexpr int analysisHop = 256;
        static constexpr float minimumFrequencyHz = 55.0f;
        static constexpr float maximumFrequencyHz = 1200.0f;
        static constexpr float levelGateDb = -60.0f;
        static constexpr int invalidHopsBeforePublish = 3;

        void resetAnalysis () noexcept;
        void analyse () noexcept;
        void publish (const TunerReading& reading) noexcept;
        void publishInvalid () noexcept;

        static void
        noteForFrequency (float frequency, float reference, int& midiNote, float& cents) noexcept;

        std::atomic<float> referenceHz{defaultReferenceHz};
        std::atomic<bool> valid{false};
        std::atomic<float> frequencyHz{0.0f};
        std::atomic<int> midiNote{-1};
        std::atomic<float> cents{0.0f};
        std::atomic<float> confidence{0.0f};
        std::atomic<float> levelDb{-120.0f};

        double sampleRate = 48000.0;
        int decimation = 4;
        int decimatedRate = 12000;
        int minimumLag = 10;
        int maximumLag = 219;
        bool prepared = false;

        std::array<float, ringSize> ring{};
        int ringWrite = 0;
        int decimationCounter = 0;
        int samplesSinceAnalysis = 0;
        float lowPassState = 0.0f;
        float lowPassCoefficient = 0.2f;
        float peakState = 0.0f;

        std::array<float, analysisSize> analysis{};
        std::array<float, maximumLagSamples + 2> difference{};
        std::array<float, maximumLagSamples + 2> normalized{};

        std::array<float, 3> frequencyHistory{};
        int historyCount = 0;
        int invalidHops = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tuner)
    };

}
