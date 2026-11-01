#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace AudioEngine
{

class MasterLimiter
{
public:
    void prepare (double sampleRate,
                  float thresholdDb = -0.3f,
                  float releaseMilliseconds = 50.0f) noexcept;

    void reset() noexcept;

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    void setThresholdDb (float thresholdDb) noexcept;
    float getThresholdDb() const noexcept { return thresholdInDb; }

    float getGainReductionDb() const noexcept;

    float getCurrentGain() const noexcept { return gain; }

private:
    float threshold = 0.9660509f;
    float thresholdInDb = -0.3f;
    float gain = 1.0f;
    float releaseCoefficient = 0.0f;
};

}
