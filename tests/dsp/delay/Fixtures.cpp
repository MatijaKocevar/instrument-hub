#include "Fixtures.h"

namespace DspTest::Delay
{

void processInBlocks (ihub::dsp::Delay& delay,
                      juce::AudioBuffer<float>& buffer,
                      int blockSize)
{
    juce::MidiBuffer midi;

    for (int position = 0; position < buffer.getNumSamples(); position += blockSize)
    {
        const auto numSamples =
            juce::jmin (blockSize, buffer.getNumSamples() - position);

        juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(),
                                        position,
                                        numSamples);

        delay.processBlock (block, midi);
    }
}

juce::AudioBuffer<float> makeImpulse (int numChannels, int numSamples, int impulseIndex)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();

    for (int channel = 0; channel < numChannels; ++channel)
        buffer.setSample (channel, impulseIndex, 1.0f);

    return buffer;
}

int peakIndex (const juce::AudioBuffer<float>& buffer, int channel,
               int from, int to)
{
    int index = -1;
    float peak = 0.0f;

    for (int i = from; i < to; ++i)
    {
        const auto magnitude = std::abs (buffer.getSample (channel, i));

        if (magnitude > peak)
        {
            peak = magnitude;
            index = i;
        }
    }

    return index;
}

int countNonZero (const juce::AudioBuffer<float>& buffer, int channel,
                  int from, int to, float threshold)
{
    int count = 0;

    for (int i = from; i < to; ++i)
        if (std::abs (buffer.getSample (channel, i)) > threshold)
            ++count;

    return count;
}

void configure (ihub::dsp::Delay& delay,
                ihub::dsp::DelayDivision division,
                double bpm,
                float feedback,
                float mix)
{
    ihub::dsp::DelaySettings settings;
    settings.tempoSync = true;
    settings.division = division;
    settings.feedback = feedback;
    settings.mix = mix;
    settings.dampingHz = 20000.0f;

    delay.setSettings (settings);

    ihub::dsp::DelayTransport transport;
    transport.playing = true;
    transport.bpm = bpm;
    transport.ppq = 0.0;
    delay.setTransport (transport);
}

int measureEcho (ihub::dsp::Delay& delay, int impulseIndex, int numSamples)
{
    auto buffer = makeImpulse (1, numSamples, impulseIndex);
    processInBlocks (delay, buffer);

    const auto peak = peakIndex (buffer, 0, 0, numSamples);

    if (peak < 0)
        return -1;

    return peak - impulseIndex;
}

} // namespace DspTest::Delay
