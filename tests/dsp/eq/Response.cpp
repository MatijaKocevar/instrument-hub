#include "Response.h"

#include "Fixtures.h"

namespace DspTest::Eq
{

void runResponse()
{
    std::cout << "\n-- magnitude response vs analytic RBJ target --" << std::endl;

    ihub::dsp::Eq eq;
    eq.prepareToPlay (kSampleRate, kBlockSize);

    const auto bands = makeTestBands();
    eq.setAllBands (bands);

    check (eq.getBand (0) == bands[0], "band 0 settings are stored");
    check (eq.getBand (1) == bands[1], "band 1 settings are stored");
    check (eq.getBand (2) == bands[2], "band 2 settings are stored");

    // Three test frequencies: low-shelf region, bell centre, high-shelf region.
    constexpr std::array<double, 3> testFrequencies { 50.0, 1000.0, 10000.0 };

    for (const auto frequency : testFrequencies)
    {
        const auto expected = targetMagnitudeDb (bands, kSampleRate, frequency);
        const auto measured = measureMagnitudeDb (eq, kSampleRate, frequency);
        const auto error = std::abs (measured - expected);

        std::cout << "      " << frequency << " Hz: measured "
                  << measured << " dB, target " << expected << " dB, error "
                  << error << " dB" << std::endl;

        check (error <= kToleranceDb,
               "magnitude at " + std::to_string (static_cast<int> (frequency))
                   + " Hz is within 0.5 dB of the analytic target");
    }

    // The settings are far from unity; a dead EQ would fail the checks above,
    // but report the intended shape explicitly as well.
    check (std::abs (measureMagnitudeDb (eq, kSampleRate, 50.0)
                     - targetMagnitudeDb (bands, kSampleRate, 50.0)) <= kToleranceDb,
           "low shelf lifts the low band");
    check (measureMagnitudeDb (eq, kSampleRate, 50.0) > 3.0,
           "50 Hz actually gets a low-shelf boost");
    check (measureMagnitudeDb (eq, kSampleRate, 1000.0) < -2.0,
           "1 kHz actually gets the bell cut");
}

} // namespace DspTest::Eq
