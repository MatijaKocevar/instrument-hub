#include "Measurements.h"

namespace SamplerTest
{

double estimateFrequency (const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          int channel)
{
    const int frames = buffer.getNumSamples();

    if (frames < 8)
        return 0.0;

    const int start = frames / 10;
    const int end = frames - frames / 10;
    const auto* data = buffer.getReadPointer (channel);

    double firstCrossing = -1.0;
    double lastCrossing = -1.0;
    int crossings = 0;

    for (int i = start + 1; i < end; ++i)
    {
        const auto previous = data[i - 1];
        const auto current = data[i];

        if (previous <= 0.0f && current > 0.0f)
        {
            const auto fraction = (double) -previous / (double) (current - previous);
            const auto position = (double) (i - 1) + fraction;

            if (firstCrossing < 0.0)
                firstCrossing = position;

            lastCrossing = position;
            ++crossings;
        }
    }

    if (crossings < 2 || lastCrossing <= firstCrossing)
        return 0.0;

    return (double) (crossings - 1) * sampleRate / (lastCrossing - firstCrossing);
}

double meanOfMiddle (const juce::AudioBuffer<float>& buffer, int channel)
{
    const int frames = buffer.getNumSamples();
    const int start = frames / 10;
    const int end = frames - frames / 10;

    if (end <= start)
        return 0.0;

    const auto* data = buffer.getReadPointer (channel);
    double sum = 0.0;

    for (int i = start; i < end; ++i)
        sum += (double) data[i];

    return sum / (double) (end - start);
}

} // namespace SamplerTest
