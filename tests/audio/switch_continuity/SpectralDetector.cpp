#include "SpectralDetector.h"

#include "Constants.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace AudioTest::SwitchContinuity
{

namespace
{

void fftInPlace (std::vector<double>& re, std::vector<double>& im)
{
    const std::size_t n = re.size();

    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;

        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }

    for (std::size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = -juce::MathConstants<double>::twoPi
                             / static_cast<double> (length);
        const double wLengthRe = std::cos (angle);
        const double wLengthIm = std::sin (angle);

        for (std::size_t start = 0; start < n; start += length)
        {
            double wRe = 1.0;
            double wIm = 0.0;

            for (std::size_t offset = 0; offset < length / 2; ++offset)
            {
                const std::size_t even = start + offset;
                const std::size_t odd = even + length / 2;

                const double evenRe = re[even];
                const double evenIm = im[even];
                const double oddRe = re[odd] * wRe - im[odd] * wIm;
                const double oddIm = re[odd] * wIm + im[odd] * wRe;

                re[even] = evenRe + oddRe;
                im[even] = evenIm + oddIm;
                re[odd] = evenRe - oddRe;
                im[odd] = evenIm - oddIm;

                const double nextWRe = wRe * wLengthRe - wIm * wLengthIm;
                wIm = wRe * wLengthIm + wIm * wLengthRe;
                wRe = nextWRe;
            }
        }
    }
}

} // namespace

SpectralReport analyseSpectrum (const std::vector<float>& signal)
{
    SpectralReport report;

    constexpr int frameSize = kSpectralFrameSize;
    constexpr int hop = frameSize / 2;
    constexpr int firstHighBin = static_cast<int> (
        std::ceil (kSpectralCutoffHz * frameSize / kSampleRate));

    std::vector<double> window (frameSize);
    std::vector<double> re (frameSize);
    std::vector<double> im (frameSize);

    for (int i = 0; i < frameSize; ++i)
        window[static_cast<size_t> (i)] = 0.5
            - 0.5 * std::cos (juce::MathConstants<double>::twoPi
                              * static_cast<double> (i)
                              / static_cast<double> (frameSize - 1));

    const int sampleCount = static_cast<int> (signal.size());

    constexpr std::array<double, 6> diagnosticThresholds { -70.0, -60.0, -50.0,
                                                           -40.0, -30.0, -20.0 };

    for (int start = 0; start + frameSize <= sampleCount; start += hop)
    {
        for (int i = 0; i < frameSize; ++i)
        {
            re[static_cast<size_t> (i)] =
                static_cast<double> (signal[static_cast<size_t> (start + i)])
                * window[static_cast<size_t> (i)];
            im[static_cast<size_t> (i)] = 0.0;
        }

        fftInPlace (re, im);

        double totalEnergy = 0.0;
        double highEnergy = 0.0;

        // Skip DC (bin 0): the Hann window's mean is not splatter.
        for (int bin = 1; bin < frameSize / 2; ++bin)
        {
            const double magnitudeSquared = re[static_cast<size_t> (bin)] * re[static_cast<size_t> (bin)]
                                          + im[static_cast<size_t> (bin)] * im[static_cast<size_t> (bin)];

            totalEnergy += magnitudeSquared;

            if (bin >= firstHighBin)
                highEnergy += magnitudeSquared;
        }

        const double splatterDb = 10.0 * std::log10 ((highEnergy + kEnergyFloor)
                                                     / (totalEnergy + kEnergyFloor));

        ++report.framesAnalysed;

        for (std::size_t band = 0; band < diagnosticThresholds.size(); ++band)
            if (splatterDb > diagnosticThresholds[band])
                ++report.countsAbove[band];

        if (splatterDb > report.maxSplatterDb)
        {
            report.maxSplatterDb = splatterDb;
            report.worstFrame = start;
        }

        if (splatterDb > kSpectralThresholdDb)
            ++report.detections;
    }

    return report;
}

} // namespace AudioTest::SwitchContinuity
