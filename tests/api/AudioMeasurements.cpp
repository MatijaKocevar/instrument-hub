#include "AudioMeasurements.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

namespace ApiTest
{

void renderBlocks (ihub::Session& session, int blocks, int blockSize)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < blocks; ++block)
        session.render (buffer, midi, blockSize);
}

std::vector<float> capture (ihub::Session& session,
                            int settleBlocks,
                            int measureBlocks,
                            int blockSize)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> samples;
    samples.reserve (static_cast<std::size_t> (measureBlocks * blockSize));

    for (int block = 0; block < settleBlocks + measureBlocks; ++block)
    {
        session.render (buffer, midi, blockSize);

        if (block < settleBlocks)
            continue;

        for (int frame = 0; frame < blockSize; ++frame)
            samples.push_back (buffer.getSample (0, frame));
    }

    return samples;
}

double rmsOf (const std::vector<float>& samples)
{
    if (samples.empty())
        return 0.0;

    double sum = 0.0;

    for (const float sample : samples)
        sum += static_cast<double> (sample) * static_cast<double> (sample);

    return std::sqrt (sum / static_cast<double> (samples.size()));
}

double dbfs (double linear)
{
    return 20.0 * std::log10 (juce::jmax (1.0e-12, linear));
}

double maxDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    const std::size_t count = std::min (a.size(), b.size());
    double maximum = 0.0;

    for (std::size_t index = 0; index < count; ++index)
        maximum = juce::jmax (maximum,
                              std::abs (static_cast<double> (a[index])
                                        - static_cast<double> (b[index])));

    return maximum;
}

double estimateFrequency (const std::vector<float>& samples, double sampleRate)
{
    if (samples.size() < 2)
        return 0.0;

    int crossings = 0;

    for (std::size_t index = 1; index < samples.size(); ++index)
    {
        if (samples[index - 1] < 0.0f && samples[index] >= 0.0f)
            ++crossings;
    }

    const double seconds = static_cast<double> (samples.size()) / sampleRate;

    return seconds > 0.0 ? static_cast<double> (crossings) / seconds : 0.0;
}

double captureFrequency (ihub::Session& session,
                         double seconds,
                         double sampleRate,
                         int blockSize)
{
    const int total = static_cast<int> (std::lround (seconds * sampleRate / blockSize))
                      * blockSize;

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    std::vector<float> captured;
    captured.reserve (static_cast<std::size_t> (total));

    for (int done = 0; done < total; done += blockSize)
    {
        session.render (buffer, midi, blockSize);

        for (int frame = 0; frame < blockSize; ++frame)
            captured.push_back (buffer.getSample (0, frame));
    }

    int crossings = 0;

    for (std::size_t index = 1; index < captured.size(); ++index)
    {
        if (captured[index - 1] <= 0.0f && captured[index] > 0.0f)
            ++crossings;
    }

    return static_cast<double> (crossings) * sampleRate
           / static_cast<double> (captured.size());
}

} // namespace ApiTest
