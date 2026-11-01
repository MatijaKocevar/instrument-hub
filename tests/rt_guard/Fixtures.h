#pragma once

#include "audio/AudioEngine.h"

#include <string>

namespace RtGuardTest
{

extern int failures;

void check (bool condition, const std::string& what);

enum class Injection
{
    none,
    allocation,
    lock,
    log
};

const char* toString (Injection injection) noexcept;

/** A sine source that can break one rule of the audio-thread contract. */
class ProbeSource final : public AudioEngine::SlotSource
{
public:
    explicit ProbeSource (Injection injectionToUse);

    void prepare (double newSampleRate, int) override;
    void setArmed (bool shouldBeArmed) noexcept { armed = shouldBeArmed; }
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    Injection injection;
    juce::String logText;

    double sampleRate = 48000.0;
    std::int64_t position = 0;
    bool armed = false;
};

/** Engine + probe + pre-allocated buffers, wired exactly like the app. */
struct Harness
{
    explicit Harness (Injection injection);

    void render (int blocks);

    static constexpr int blockSize = 128;

    AudioEngine::AudioEngine engine { 2, 2 };
    ProbeSource source;
    juce::AudioBuffer<float> output;
    juce::MidiBuffer midi;
};

struct AbortResult
{
    bool aborted = false;
    std::string output;
};

AbortResult runStrictChild (Injection injection);

} // namespace RtGuardTest
