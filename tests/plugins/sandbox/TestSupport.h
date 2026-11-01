#pragma once

#include "audio/AudioEngine.h"
#include "plugins/SandboxHost.h"

#include <string>

namespace SandboxTest
{

extern int failures;

void expect (bool condition, const juce::String& description);
float rmsOf (const juce::AudioBuffer<float>& buffer);

/** A healthy slot that must survive the sandbox dying. */
class SteadySineSource final : public AudioEngine::SlotSource
{
public:
    void prepare (double sampleRate, int) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    double phase = 0.0;
    double increment = 0.0;
};

ihub::plugins::SandboxHost::Options makeFakeOptions (bool abortOnPurpose);

} // namespace SandboxTest
