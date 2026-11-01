#include "pad_audio/Metrics.h"
#include "pad_audio/Constants.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace PadAudio
{

double toDbfs (double linear) noexcept
{
    return 20.0 * std::log10 (juce::jmax (linear, 1.0e-12));
}

double rmsDbfs (const std::vector<float>& samples, int first, int last)
{
    first = juce::jlimit (0, static_cast<int> (samples.size()), first);
    last = juce::jlimit (first, static_cast<int> (samples.size()), last);

    double sumSquares = 0.0;

    for (int index = first; index < last; ++index)
        sumSquares += static_cast<double> (samples[static_cast<std::size_t> (index)])
                      * static_cast<double> (samples[static_cast<std::size_t> (index)]);

    const int count = last - first;

    if (count <= 0)
        return -1000.0;

    return toDbfs (std::sqrt (sumSquares / static_cast<double> (count)));
}

double maxDelta (const std::vector<float>& samples, int first, int last)
{
    first = juce::jlimit (0, static_cast<int> (samples.size()), first);
    last = juce::jlimit (first, static_cast<int> (samples.size()), last);

    double maximum = 0.0;

    for (int index = juce::jmax (1, first); index < last; ++index)
        maximum = juce::jmax (maximum,
                              std::abs (static_cast<double> (samples[static_cast<std::size_t> (index)])
                                        - static_cast<double> (samples[static_cast<std::size_t> (index - 1)])));

    return maximum;
}

double toneMagnitude (const std::vector<float>& samples,
                      int first,
                      int last,
                      double frequency)
{
    first = juce::jlimit (0, static_cast<int> (samples.size()), first);
    last = juce::jlimit (first, static_cast<int> (samples.size()), last);

    const int count = last - first;

    if (count <= 0)
        return 0.0;

    double real = 0.0;
    double imaginary = 0.0;

    for (int index = first; index < last; ++index)
    {
        const double phase = juce::MathConstants<double>::twoPi * frequency
                             * static_cast<double> (index - first) / kSampleRate;
        const double value = static_cast<double> (samples[static_cast<std::size_t> (index)]);

        real += value * std::cos (phase);
        imaginary += value * std::sin (phase);
    }

    return std::sqrt (real * real + imaginary * imaginary) / static_cast<double> (count);
}

} // namespace PadAudio
