#include "MakeupGain.h"

#include "Fixtures.h"

namespace DspTest::Compressor
{

void runMakeupGain()
{
    std::cout << "\n-- makeup gain --" << std::endl;

    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    ihub::dsp::CompressorSettings settings;
    settings.thresholdDb = -18.0f;
    settings.ratio = 4.0f;
    settings.makeupDb = 6.0f;
    compressor.setSettings (settings);

    // Below threshold: the output is the input + makeup.
    const auto lowGainDb = measureSteadyGainDb (compressor, -30.0);
    std::cout << "      below threshold: " << lowGainDb << " dB (expected 6 dB)"
              << std::endl;
    check (std::abs (lowGainDb - 6.0) <= 0.01,
           "makeup applies exactly +6 dB below threshold");

    // Above threshold: the static curve is unchanged, makeup is on top of it.
    const auto highGainDb = measureSteadyGainDb (compressor, -12.0);
    const auto highGrDb = -(highGainDb - 6.0);
    std::cout << "      6 dB over: " << highGainDb << " dB net, " << highGrDb
              << " dB of gain reduction" << std::endl;
    check (std::abs (highGrDb - 4.5) <= 1.0,
           "makeup does not change the 4.5 dB gain reduction above threshold");

    // Negative makeup is just a cut.
    settings.makeupDb = -3.0f;
    compressor.setSettings (settings);
    const auto cutGainDb = measureSteadyGainDb (compressor, -30.0);
    check (std::abs (cutGainDb + 3.0) <= 0.01,
           "makeup applies exactly -3 dB below threshold");
}

} // namespace DspTest::Compressor
