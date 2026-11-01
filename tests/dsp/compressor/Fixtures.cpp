#include "Fixtures.h"

#include <cmath>

namespace DspTest::Compressor
{

double amplitudeForDb (double levelDb)
{
    return std::pow (10.0, levelDb / 20.0);
}

void fillTone (juce::AudioBuffer<float>& buffer, int channel,
               double levelDb, double frequency, double sampleRate)
{
    const auto amplitude = static_cast<float> (amplitudeForDb (levelDb));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (channel, i,
                          amplitude
                              * static_cast<float> (std::sin (
                                  juce::MathConstants<double>::twoPi * frequency
                                  * static_cast<double> (i) / sampleRate)));
}

void processInBlocks (ihub::dsp::Compressor& compressor,
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

        compressor.processBlock (block, midi);
    }
}

double rmsDb (const juce::AudioBuffer<float>& buffer, int channel, int from, int to)
{
    double sum = 0.0;

    for (int i = from; i < to; ++i)
    {
        const auto value = static_cast<double> (buffer.getSample (channel, i));
        sum += value * value;
    }

    return juce::Decibels::gainToDecibels (std::sqrt (sum / static_cast<double> (to - from)),
                                           -300.0);
}

double measureSteadyGainDb (ihub::dsp::Compressor& compressor,
                            double levelDb,
                            double frequency,
                            double sampleRate,
                            double seconds)
{
    const auto numSamples = static_cast<int> (seconds * sampleRate);

    juce::AudioBuffer<float> signal (1, numSamples);
    fillTone (signal, 0, levelDb, frequency, sampleRate);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (compressor, signal, kBlockSize);

    const auto from = static_cast<int> (0.6 * sampleRate);
    const auto inputDb = rmsDb (reference, 0, from, numSamples);
    const auto outputDb = rmsDb (signal, 0, from, numSamples);

    return outputDb - inputDb;
}

} // namespace DspTest::Compressor
