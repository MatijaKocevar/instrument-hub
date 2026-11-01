#include "Feedback.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runFeedback()
{
    std::cout << "\n-- feedback repeats --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0, 0.5f, 1.0f);

    constexpr int impulseIndex = 16;
    constexpr int delaySamples = 12000;
    const auto numSamples = impulseIndex + 3 * delaySamples + 64;

    auto buffer = makeImpulse (1, numSamples, impulseIndex);
    processInBlocks (delay, buffer);

    const auto first = peakIndex (buffer, 0, 0, numSamples);
    const auto second = peakIndex (buffer, 0, first + 1, numSamples);
    const auto third = peakIndex (buffer, 0, second + 1, numSamples);

    std::cout << "      peaks: " << first << ", " << second << ", " << third
              << std::endl;

    check (first == impulseIndex + delaySamples, "first repeat is one delay later");
    check (second == first + delaySamples, "second repeat is two delays later");
    check (third == second + delaySamples, "third repeat is three delays later");

    const auto level1 = std::abs (buffer.getSample (0, first));
    const auto level2 = std::abs (buffer.getSample (0, second));
    const auto level3 = std::abs (buffer.getSample (0, third));

    std::cout << "      levels: " << level1 << ", " << level2 << ", " << level3
              << std::endl;

    check (level2 < level1 && level3 < level2,
           "successive repeats decay with feedback");
}

} // namespace DspTest::Delay
