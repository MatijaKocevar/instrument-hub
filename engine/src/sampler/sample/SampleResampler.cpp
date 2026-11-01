#include "SampleResampler.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>
#include <vector>

namespace ihub::sampler::sample_detail
{

namespace
{

constexpr int resamplerZeroCrossings = 24;

constexpr int resamplerPhases = 512;

struct SincTable
{
    SincTable (double sourceRate, double destinationRate)
    {
        const auto nyquistRatio = destinationRate / juce::jmax (1.0, sourceRate);
        cutoff = juce::jmin (0.5, 0.5 * nyquistRatio);

        halfWidth = juce::jmax (1, (int) std::ceil (resamplerZeroCrossings / (2.0 * cutoff)));
        taps = halfWidth * 2;

        weights.resize ((size_t) (resamplerPhases + 1) * (size_t) taps);

        const double twoFc = 2.0 * cutoff;

        for (int phase = 0; phase <= resamplerPhases; ++phase)
        {
            const double fraction = (double) phase / (double) resamplerPhases;
            float* row = weights.data() + (size_t) phase * (size_t) taps;
            double sum = 0.0;

            for (int tap = 0; tap < taps; ++tap)
            {
                const double x = fraction + (double) halfWidth - 1.0 - (double) tap;

                double sinc = 1.0;

                if (std::abs (x) > 1.0e-12)
                    sinc = std::sin (juce::MathConstants<double>::pi * twoFc * x)
                           / (juce::MathConstants<double>::pi * twoFc * x);

                const double window = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * x
                                                             / (double) halfWidth);

                const auto weight = (float) (twoFc * sinc * window);
                row[tap] = weight;
                sum += (double) weight;
            }

            if (std::abs (sum) > 1.0e-12)
            {
                const auto scale = (float) (1.0 / sum);

                for (int tap = 0; tap < taps; ++tap)
                    row[tap] *= scale;
            }
        }
    }

    double cutoff = 0.5;
    int halfWidth = 0;
    int taps = 0;
    std::vector<float> weights;
};

}

juce::AudioBuffer<float> resampleBuffer (const juce::AudioBuffer<float>& source,
                                         double sourceRate,
                                         double destinationRate)
{
    const int numChannels = source.getNumChannels();
    const int sourceFrames = source.getNumSamples();

    if (numChannels <= 0 || sourceFrames <= 0)
        return {};

    if (std::abs (sourceRate - destinationRate) < 1.0e-9)
    {
        juce::AudioBuffer<float> copy (numChannels, sourceFrames);
        copy.makeCopyOf (source);
        return copy;
    }

    const auto roundedLength = std::llround ((double) sourceFrames * destinationRate / sourceRate);
    const int outputFrames = (int) juce::jlimit ((juce::int64) 1,
                                                 (juce::int64) std::numeric_limits<int>::max(),
                                                 (juce::int64) roundedLength);

    juce::AudioBuffer<float> destination (numChannels, outputFrames);
    destination.clear();

    const SincTable table (sourceRate, destinationRate);
    const double step = sourceRate / destinationRate;
    const int taps = table.taps;
    const int halfWidth = table.halfWidth;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* input = source.getReadPointer (channel);
        float* output = destination.getWritePointer (channel);

        for (int n = 0; n < outputFrames; ++n)
        {
            const double position = (double) n * step;
            const double base = std::floor (position);
            const double fraction = position - base;

            double phasePosition = fraction * (double) resamplerPhases;
            int phase = (int) phasePosition;

            if (phase >= resamplerPhases)
            {
                phase = resamplerPhases - 1;
                phasePosition = (double) resamplerPhases;
            }

            const auto blend = (float) (phasePosition - (double) phase);
            const float* row0 = table.weights.data() + (size_t) phase * (size_t) taps;
            const float* row1 = row0 + taps;

            const auto firstInput = (juce::int64) base - halfWidth + 1;

            int firstTap = 0;

            if (firstInput < 0)
                firstTap = (int) juce::jmin ((juce::int64) taps, -firstInput);

            int lastTap = taps;

            if (firstInput + taps > sourceFrames)
                lastTap = (int) juce::jmax ((juce::int64) firstTap, (juce::int64) sourceFrames - firstInput);

            float sum = 0.0f;

            for (int tap = firstTap; tap < lastTap; ++tap)
            {
                const float weight = row0[tap] + blend * (row1[tap] - row0[tap]);
                sum += input[firstInput + tap] * weight;
            }

            output[n] = sum;
        }
    }

    return destination;
}

bool isSupportedEngineRate (double rate) noexcept
{
    return std::isfinite (rate) && rate >= 8000.0 && rate <= 384000.0;
}

}
