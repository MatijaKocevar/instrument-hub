#include "Fixtures.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace VoiceTest
{

std::string fixtureJson()
{
    return R"({
      "schemaVersion": 1,
      "name": "Pad Test Kit",
      "pads": [
        { "note": 36, "name": "Kick",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "kick.wav" } ] } ] },
        { "note": 42, "name": "Closed Hat",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "closed-hat.wav" } ] } ] },
        { "note": 46, "name": "Open Hat",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "open-hat.wav" } ] } ] },
        { "note": 50, "name": "Gate Pad",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "gate.wav" } ] } ] }
      ]
    })";
}

juce::AudioBuffer<float> render (ihub::sampler::SamplerInstrument& instrument,
                                 int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();
    juce::MidiBuffer midi;
    instrument.process (buffer, midi);
    return buffer;
}

float peakLevel (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

    return peak;
}

float peakLevel (const juce::AudioBuffer<float>& buffer, int start, int count)
{
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax (peak, buffer.getMagnitude (channel, start, count));

    return peak;
}

}
