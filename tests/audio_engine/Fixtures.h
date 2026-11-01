#pragma once

#include "audio/AudioEngine.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AudioEngineTest
{

extern int failures;

void check (bool condition, const std::string& what);
double dbfs (double linear);

class SineSource final : public AudioEngine::SlotSource
{
public:
    SineSource (double frequencyToUse, float amplitudeToUse, double phaseToUse = 0.0);

    void prepare (double newSampleRate, int) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    double referenceAt (std::int64_t index) const;

    int latencySamples() const noexcept override { return latency; }

    std::atomic<int> processCalls { 0 };
    int latency = 0;

private:
    double valueAt (std::int64_t index) const;

    double sampleRate = 48000.0;
    double frequency = 440.0;
    float amplitude = 0.1f;
    double phase = 0.0;
    std::int64_t position = 0;
};

double referenceSample (const std::vector<std::unique_ptr<SineSource>>& sources,
                        const std::vector<float>& gains,
                        std::int64_t index);

}
