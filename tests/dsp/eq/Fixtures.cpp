#include "Fixtures.h"

namespace DspTest::Eq
{

TargetBiquad designTarget (ihub::dsp::EqBandType type,
                           double sampleRate,
                           double frequency,
                           double q,
                           double gainDb,
                           bool enabled)
{
    if (! enabled || std::abs (gainDb) < 1.0e-9)
        return {};

    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto cosw0 = std::cos (w0);
    const auto sinw0 = std::sin (w0);

    double n0 = 1.0, n1 = 0.0, n2 = 0.0;
    double d0 = 1.0, d1 = 0.0, d2 = 0.0;

    if (type == ihub::dsp::EqBandType::peak)
    {
        const auto alpha = sinw0 / (2.0 * q);

        n0 = 1.0 + alpha * A;
        n1 = -2.0 * cosw0;
        n2 = 1.0 - alpha * A;

        d0 = 1.0 + alpha / A;
        d1 = -2.0 * cosw0;
        d2 = 1.0 - alpha / A;
    }
    else
    {
        // Q form of the shelf slope: beta == 2 * sqrt(A) * alpha.
        const auto beta = sinw0 * std::sqrt (A) / q;
        const auto aPlus1 = A + 1.0;
        const auto aMinus1 = A - 1.0;
        const auto aMinus1Cos = aMinus1 * cosw0;

        if (type == ihub::dsp::EqBandType::lowShelf)
        {
            n0 = A * (aPlus1 - aMinus1Cos + beta);
            n1 = 2.0 * A * (aMinus1 - aPlus1 * cosw0);
            n2 = A * (aPlus1 - aMinus1Cos - beta);

            d0 = aPlus1 + aMinus1Cos + beta;
            d1 = -2.0 * (aMinus1 + aPlus1 * cosw0);
            d2 = aPlus1 + aMinus1Cos - beta;
        }
        else // highShelf
        {
            n0 = A * (aPlus1 + aMinus1Cos + beta);
            n1 = -2.0 * A * (aMinus1 + aPlus1 * cosw0);
            n2 = A * (aPlus1 + aMinus1Cos - beta);

            d0 = aPlus1 - aMinus1Cos + beta;
            d1 = 2.0 * (aMinus1 - aPlus1 * cosw0);
            d2 = aPlus1 - aMinus1Cos - beta;
        }
    }

    TargetBiquad result;
    result.b0 = n0 / d0;
    result.b1 = n1 / d0;
    result.b2 = n2 / d0;
    result.a1 = d1 / d0;
    result.a2 = d2 / d0;
    return result;
}

double targetMagnitudeDb (const std::array<ihub::dsp::EqBandSettings,
                                           ihub::dsp::Eq::numBands>& bands,
                          double sampleRate,
                          double frequency)
{
    const auto omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto z1 = std::polar (1.0, -omega);
    const auto z2 = z1 * z1;

    std::complex<double> response (1.0, 0.0);

    for (const auto& band : bands)
    {
        const auto biquad = designTarget (band.type, sampleRate, band.frequencyHz,
                                          band.q, band.gainDb, band.enabled);

        const auto numerator = biquad.b0 + biquad.b1 * z1 + biquad.b2 * z2;
        const auto denominator = 1.0 + biquad.a1 * z1 + biquad.a2 * z2;

        response *= numerator / denominator;
    }

    return juce::Decibels::gainToDecibels (std::abs (response), -300.0);
}

double measureMagnitudeDb (ihub::dsp::Eq& eq, double sampleRate, double frequency)
{
    juce::AudioBuffer<float> response (1, kImpulseLength);
    response.clear();
    response.setSample (0, 0, 1.0f);

    eq.reset();

    juce::MidiBuffer midi;
    eq.processBlock (response, midi);

    std::complex<double> sum (0.0, 0.0);
    const auto step = -juce::MathConstants<double>::twoPi * frequency / sampleRate;

    // Recurrence keeps the phase coherent without 32768 trig calls.
    const auto rotation = std::polar (1.0, step);
    std::complex<double> phase (1.0, 0.0);

    for (int n = 0; n < kImpulseLength; ++n)
    {
        sum += static_cast<double> (response.getSample (0, n)) * phase;
        phase *= rotation;
    }

    return juce::Decibels::gainToDecibels (std::abs (sum), -300.0);
}

std::array<ihub::dsp::EqBandSettings, ihub::dsp::Eq::numBands> makeTestBands()
{
    using ihub::dsp::EqBandSettings;
    using ihub::dsp::EqBandType;

    return { EqBandSettings { EqBandType::lowShelf, 100.0f, 6.0f, 0.7071068f, true },
             EqBandSettings { EqBandType::peak, 1000.0f, -4.0f, 1.0f, true },
             EqBandSettings { EqBandType::highShelf, 8000.0f, 3.0f, 0.7071068f, true } };
}

} // namespace DspTest::Eq
