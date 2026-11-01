#pragma once

#include "audio/AudioEngine.h"

#include <cstdint>

namespace ihub::app::detail
{

enum class RtInjection
{
    none,
    allocation,
    lock,
    log
};

const char* toString (RtInjection injection) noexcept;

class RtProbeSource final : public AudioEngine::SlotSource
{
public:
    explicit RtProbeSource (RtInjection injectionToUse);

    void prepare (double newSampleRate, int maximumBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    void setArmed (bool shouldBeArmed) noexcept { armed = shouldBeArmed; }

private:
    RtInjection injection;
    juce::String logText;

    double sampleRate = 48000.0;
    double frequency = 440.0;
    double amplitude = 0.1;
    std::int64_t position = 0;
    bool armed = false;
};

}
