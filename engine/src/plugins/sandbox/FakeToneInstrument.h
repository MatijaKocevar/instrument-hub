#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub::plugins::sandbox_client
{
/** The test seam: a fake instrument that needs no VST3.

    It renders a steady tone so the host's shared-memory transport can be
    verified end to end. `--abort-after N` makes the fake plugin call
    `std::abort()` on block N+1, which is exactly the hostile-plugin case the
    sandbox exists for: only this process dies. */
class FakeToneInstrument
{
public:
    FakeToneInstrument (double sampleRateToUse, double frequency, int abortAfter);

    int latencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

private:
    double increment;
    int abortAfterBlocks;
    double phase = 0.0;
    int blocksRendered = 0;
};

} // namespace ihub::plugins::sandbox_client
