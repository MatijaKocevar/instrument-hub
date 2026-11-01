#include "StaticCurve.h"

#include "Fixtures.h"

namespace DspTest::Compressor
{

void runStaticCurve()
{
    std::cout << "\n-- static curve: 6 dB over threshold --" << std::endl;

    constexpr double thresholdDb = -18.0;

    struct Case { double ratio; double overDb; };
    const std::array<Case, 6> cases {
        Case { 2.0, 3.0 }, Case { 2.0, 6.0 },
        Case { 4.0, 3.0 }, Case { 4.0, 6.0 },
        Case { 4.0, 12.0 }, Case { 8.0, 6.0 }
    };

    for (const auto& testCase : cases)
    {
        ihub::dsp::Compressor compressor;
        compressor.prepareToPlay (kSampleRate, kBlockSize);

        ihub::dsp::CompressorSettings settings;
        settings.thresholdDb = static_cast<float> (thresholdDb);
        settings.ratio = static_cast<float> (testCase.ratio);
        settings.attackMs = 10.0f;
        settings.releaseMs = 100.0f;
        settings.makeupDb = 0.0f;
        compressor.setSettings (settings);

        const auto inputDb = thresholdDb + testCase.overDb;
        const auto measuredGainDb = measureSteadyGainDb (compressor, inputDb);
        const auto expectedGrDb = testCase.overDb * (1.0 - 1.0 / testCase.ratio);
        const auto measuredGrDb = -measuredGainDb;
        const auto error = std::abs (measuredGrDb - expectedGrDb);

        std::cout << "      ratio " << number (testCase.ratio) << ":1, "
                  << number (testCase.overDb) << " dB over: expected "
                  << number (expectedGrDb) << " dB, measured " << number (measuredGrDb)
                  << " dB, error " << number (error) << " dB" << std::endl;

        check (error <= kStaticToleranceDb,
               "ratio " + number (testCase.ratio) + ":1 at " + number (testCase.overDb)
                   + " dB over threshold reduces gain by the static curve within 1 dB");

        check (std::abs (compressor.getGainReductionDb() - expectedGrDb) <= kStaticToleranceDb,
               "the reported gain reduction matches the static curve within 1 dB");
    }

    // The literal acceptance sentence: 6 dB over threshold with the default
    // 4:1 ratio means 4.5 dB of gain reduction.
    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    const auto gainDb = measureSteadyGainDb (compressor, thresholdDb + 6.0);
    const auto measuredGr = -gainDb;

    std::cout << "      acceptance: 6 dB over, 4:1 -> " << measuredGr
              << " dB (expected 4.5 dB)" << std::endl;

    check (std::abs (measuredGr - 4.5) <= 1.0,
           "6 dB over threshold yields 4.5 dB of gain reduction within 1 dB");

    // Below threshold: unity gain, no gain reduction.
    const auto belowGainDb = measureSteadyGainDb (compressor, thresholdDb - 6.0);
    check (std::abs (belowGainDb) <= 0.01,
           "a signal below threshold passes at unity gain");
    check (std::abs (compressor.getGainReductionDb()) <= 0.01,
           "the reported gain reduction is 0 below threshold");

    // Ratio 1:1 is a no-op.
    ihub::dsp::CompressorSettings unity;
    unity.ratio = 1.0f;
    compressor.setSettings (unity);

    const auto unityGainDb = measureSteadyGainDb (compressor, thresholdDb + 12.0);
    check (std::abs (unityGainDb) <= 0.01,
           "ratio 1:1 applies no gain reduction");
}

} // namespace DspTest::Compressor
