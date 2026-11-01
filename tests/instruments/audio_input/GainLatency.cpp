#include "GainLatency.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "instruments/AudioInputInstrument.h"
#include "instruments/Instrument.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace AudioInputTest
{

void runGainAndLatency()
{
    std::cout << "\n-- optional gain and own latency accounting --" << std::endl;

    AudioInputInstrument input;
    input.setGain (0.25f);
    input.setLatencySamples (96);
    input.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> source (1, kBlockSize);
    fillSine (source, 0);

    const float* channels[] = { source.getReadPointer (0) };
    input.setInputBlock (channels, 1);

    juce::AudioBuffer<float> rendered (2, kBlockSize);
    juce::MidiBuffer midi;
    input.process (rendered, midi);

    double maxDifference = 0.0;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            maxDifference = std::max (
                maxDifference,
                std::abs (static_cast<double> (rendered.getSample (channel, i))
                          - 0.25 * static_cast<double> (source.getSample (0, i))));

    check (maxDifference < 1.0e-7, "gain 0.25 is applied to both output channels");
    check (std::abs (input.getGain() - 0.25f) <= 0.0f,
           "getGain() reports the linear gain");
    check (input.latencySamples() == 96,
           "latencySamples() reports the cached input-path latency");
    check (input.kind() == ihub::Instrument::Kind::AudioInput,
           "kind() reports Instrument::Kind::AudioInput");
    check (std::string (ihub::toString (input.kind())) == "audio-input",
           "toString() is stable for logs and tests");

    input.noteOn (1, 60, 1.0f);
    input.noteOff (1, 60);
}

} // namespace AudioInputTest
