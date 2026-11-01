#include "Acceptance.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runAcceptance()
{
    std::cout << "\n-- acceptance: 1/4 note at 120 BPM, 48 kHz --" << std::endl;

    const auto expected = ihub::dsp::Delay::delaySamplesForDivision (
        ihub::dsp::DelayDivision::quarter, 120.0, 48000.0);

    std::cout << "      pure function: " << expected << " samples" << std::endl;
    check (expected == 12000,
           "Delay::delaySamplesForDivision(quarter, 120 BPM, 48 kHz) is 12000");

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0);

    std::cout << "      reported delay: " << delay.getDelaySamples()
              << " samples (" << number (delay.getDelaySeconds() * 1000.0)
              << " ms)" << std::endl;

    check (delay.getDelaySamples() == 12000,
           "the prepared delay reports 12000 samples");

    constexpr int impulseIndex = 100;
    const auto numSamples = impulseIndex + 12000 + 512;

    auto buffer = makeImpulse (1, numSamples, impulseIndex);
    processInBlocks (delay, buffer);

    const auto echo = peakIndex (buffer, 0, 0, numSamples);
    const auto echoLevel = buffer.getSample (0, echo);

    std::cout << "      impulse at " << impulseIndex << ", echo at " << echo
              << " (distance " << echo - impulseIndex << "), level " << echoLevel
              << std::endl;

    check (echo == impulseIndex + 12000,
           "the echo lands at exactly input + 12000 samples");
    check (std::abs (echoLevel - 1.0f) < 1.0e-6f,
           "the echo passes at unity gain (100% wet, no feedback)");
    check (countNonZero (buffer, 0, 0, numSamples) == 1,
           "only the echo is non-zero: no leak before or after the repeat");
    check (delay.getLatencySamples() == 0,
           "a delay reports zero latency (the dry signal is not delayed)");
}

}
