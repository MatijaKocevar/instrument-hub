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

/** A readable source that can deliberately break the audio-thread contract.

    The sine keeps the engine on the real processing path; the injection is
    performed at the end of `process()`, exactly where a buggy instrument or
    effect would do it.
*/
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

} // namespace ihub::app::detail
