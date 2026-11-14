#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <cstdint>

namespace ihub::dsp
{

    struct CompressorSettings
    {
        float thresholdDb = -18.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        float makeupDb = 0.0f;

        bool operator== (const CompressorSettings&) const = default;
    };

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

        Compressor ();
        ~Compressor () override = default;

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
            return "IHub Compressor";
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

        void setSettings (const CompressorSettings& settings);

        CompressorSettings getSettings () const noexcept;

        void setThresholdDb (float value);
        void setRatio (float value);
        void setAttackMs (float value);
        void setReleaseMs (float value);
        void setMakeupDb (float value);

        void setEnabled (bool shouldBeEnabled);
        bool isEnabled () const noexcept;

        void setLookaheadMs (float milliseconds);
        float getLookaheadMs () const noexcept;

        int getLookaheadSamples () const noexcept
        {
            return lookaheadSamples;
        }

        float getGainReductionDb () const noexcept;

        bool isPrepared () const noexcept
        {
            return prepared;
        }

    private:
        struct Published
        {
            float thresholdDb = -18.0f;
            float slope = 0.75f;
            float attackCoeff = 0.002f;
            float releaseCoeff = 0.0002f;
            float peakDecay = 0.9998f;
            float makeupGain = 1.0f;
        };

        struct AtomicPublished
        {
            std::atomic<float> thresholdDb{-18.0f};
            std::atomic<float> slope{0.75f};
            std::atomic<float> attackCoeff{0.002f};
            std::atomic<float> releaseCoeff{0.0002f};
            std::atomic<float> peakDecay{0.9998f};
            std::atomic<float> makeupGain{1.0f};

            void store (const Published& p) noexcept
            {
                thresholdDb.store (p.thresholdDb, std::memory_order_relaxed);
                slope.store (p.slope, std::memory_order_relaxed);
                attackCoeff.store (p.attackCoeff, std::memory_order_relaxed);
                releaseCoeff.store (p.releaseCoeff, std::memory_order_relaxed);
                peakDecay.store (p.peakDecay, std::memory_order_relaxed);
                makeupGain.store (p.makeupGain, std::memory_order_relaxed);
            }

            Published load () const noexcept
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
            std::atomic<std::uint32_t> version{0};
        };

        static CompressorSettings clampSettings (const CompressorSettings& settings) noexcept;
        static float onePoleCoefficient (float timeMs, double sampleRate) noexcept;

        Published computePublished () const noexcept;
        void publishSettings () noexcept;
        bool readSnapshot (Published& destination) const noexcept;

        float advanceGain (float inputPeak, const Published& published) noexcept;

        CompressorSettings settings;
        std::atomic<bool> enabled{true};
        std::atomic<float> lookaheadMs{defaultLookaheadMs};

        double currentSampleRate = 48000.0;
        bool prepared = false;

        Snapshot snapshot;
        std::uint32_t appliedVersion = 0;
        Published applied;

        int lookaheadSamples = 0;
        int delayIndex = 0;
        juce::AudioBuffer<float> delayBuffer;

        float detectorPeak = 0.0f;
        float gainReductionDb = 0.0f;

        std::atomic<float> reportedGainReductionDb{0.0f};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compressor)
    };

}
