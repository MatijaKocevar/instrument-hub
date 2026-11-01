#include "Fixtures.h"

namespace DspTest::Reverb
{

void processInBlocks (ihub::dsp::Reverb& reverb,
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

        reverb.processBlock (block, midi);
    }
}

juce::AudioBuffer<float> renderImpulse (ihub::dsp::Reverb& reverb,
                                        double seconds,
                                        double sampleRate)
{
    const auto numSamples = static_cast<int> (seconds * sampleRate);
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);

    processInBlocks (reverb, buffer);
    return buffer;
}

double rms (const juce::AudioBuffer<float>& buffer, int channel, int from, int to)
{
    if (to <= from)
        return 0.0;

    double sum = 0.0;

    for (int i = from; i < to; ++i)
    {
        const auto value = static_cast<double> (buffer.getSample (channel, i));
        sum += value * value;
    }

    return std::sqrt (sum / static_cast<double> (to - from));
}

double highFrequencyRatio (const juce::AudioBuffer<float>& buffer, int channel,
                           int from, int to)
{
    double differenceEnergy = 0.0;
    double signalEnergy = 0.0;

    for (int i = juce::jmax (1, from); i < to; ++i)
    {
        const auto current = static_cast<double> (buffer.getSample (channel, i));
        const auto previous = static_cast<double> (buffer.getSample (channel, i - 1));

        differenceEnergy += (current - previous) * (current - previous);
        signalEnergy += current * current;
    }

    if (signalEnergy <= 0.0)
        return 0.0;

    return std::sqrt (differenceEnergy / signalEnergy);
}

int tailLengthSamples (const juce::AudioBuffer<float>& buffer, int channel,
                       float relativeThreshold)
{
    float peak = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        peak = juce::jmax (peak, std::abs (buffer.getSample (channel, i)));

    const auto threshold = peak * relativeThreshold;
    int last = 0;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        if (std::abs (buffer.getSample (channel, i)) > threshold)
            last = i;

    return last;
}

ihub::dsp::ReverbSettings makeSettings (float roomSize, float damping,
                                        float width, float mix,
                                        float preDelayMs)
{
    ihub::dsp::ReverbSettings settings;
    settings.roomSize = roomSize;
    settings.damping = damping;
    settings.width = width;
    settings.mix = mix;
    settings.preDelayMs = preDelayMs;
    return settings;
}

} // namespace DspTest::Reverb
