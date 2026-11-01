#include "StereoLink.h"

#include "Fixtures.h"

namespace DspTest::Compressor
{

void runStereoLink()
{
    std::cout << "\n-- stereo linking --" << std::endl;

    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    const auto numSamples = static_cast<int> (1.5 * kSampleRate);
    juce::AudioBuffer<float> signal (2, numSamples);

    // Left is 6 dB over the -18 dBFS threshold, right is 24 dB below it.
    fillTone (signal, 0, -12.0, kToneFrequency, kSampleRate);
    fillTone (signal, 1, -42.0, 500.0, kSampleRate);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (compressor, signal, kBlockSize);

    const auto from = static_cast<int> (0.75 * kSampleRate);
    const auto leftGain = rmsDb (signal, 0, from, numSamples) - rmsDb (reference, 0, from, numSamples);
    const auto rightGain = rmsDb (signal, 1, from, numSamples) - rmsDb (reference, 1, from, numSamples);

    std::cout << "      left " << leftGain << " dB, right " << rightGain << " dB"
              << std::endl;

    check (std::abs (leftGain - rightGain) <= 0.05,
           "both channels receive the same gain reduction");
    check (std::abs (-leftGain - 4.5) <= 1.0,
           "the linked pair follows the loud channel's static curve");
}

} // namespace DspTest::Compressor
