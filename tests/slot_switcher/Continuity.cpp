#include "Continuity.h"
#include "Fixtures.h"

#include "audio/SlotSwitcher.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace SlotSwitcherTest
{

namespace
{

struct ContinuityResult
{
    double maxResidual = 0.0;
    double maxResidualDbfs = 0.0;
    double maxOutputJump = 0.0;
};

ContinuityResult runSwitchTest (std::vector<float>& output, bool& producedEveryBlock)
{
    const double increment = juce::MathConstants<double>::twoPi * kFrequency / kSampleRate;
    const double preRollPhase = std::fmod (kPreRollSamples * increment,
                                           juce::MathConstants<double>::twoPi);
    const double switchPhase = std::fmod (preRollPhase + kSwitchSample * increment,
                                          juce::MathConstants<double>::twoPi);

    SineSource sourceA (kAmplitude, kFrequency, 0.0);
    SineSource sourceB (kAmplitude, kFrequency, switchPhase);

    AudioEngine::Slot slotA (0);
    AudioEngine::Slot slotB (1);
    slotA.setSource (&sourceA);
    slotB.setSource (&sourceB);

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slotA, &slotB });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (switcher.getNumSlots() == 2, "two slots are attached");
    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::idle,
           "slot 0 starts idle");
    check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::idle,
           "slot 1 starts idle");

    switcher.armSlot (0);
    check (switcher.getArmedSlot() == 0, "armSlot(0) publishes the request");

    juce::MidiBuffer midi;
    producedEveryBlock = true;

    for (int block = 0; block < kPreRollBlocks; ++block)
        if (! switcher.process (kBlockSize, midi))
            producedEveryBlock = false;

    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::active,
           "slot 0 is active after its fade-in");
    check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::idle,
           "slot 1 remains idle before it is armed");
    check (std::abs (switcher.getSlotGain (0) - 1.0f) < 1.0e-6f,
           "active slot gain is 1.0");

    output.clear();
    output.reserve (static_cast<size_t> (kRecordedSamples));

    bool sawMidFade = false;

    for (int block = 0; block < kRecordedBlocks; ++block)
    {
        if (block == kSwitchBlock)
        {
            switcher.armSlot (1);
            check (switcher.getArmedSlot() == 1, "armSlot(1) publishes the switch");
        }

        if (! switcher.process (kBlockSize, midi))
            producedEveryBlock = false;

        if (block == kSwitchBlock)
        {
            check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::fading,
                   "the outgoing slot is fading during the crossfade");
            check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::warming,
                   "the incoming slot is warming during the crossfade");

            const float outGain = switcher.getSlotGain (0);
            const float inGain = switcher.getSlotGain (1);
            const float power = outGain * outGain + inGain * inGain;

            check (outGain > 0.0f && inGain > 0.0f,
                   "both slots carry signal at the first fade block");
            check (std::abs (power - 1.0f) < 1.0e-4f,
                   "mid-fade gains are equal-power (gOut^2 + gIn^2 == 1)");
            sawMidFade = true;
        }

        if (block == kSwitchBlock + 1)
        {
            check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::idle,
                   "the outgoing slot is idle once the ramp completes");
            check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::active,
                   "the incoming slot is active once the ramp completes");
        }

        const auto& mixed = switcher.getOutputBuffer();

        for (int frame = 0; frame < kBlockSize; ++frame)
            output.push_back (mixed.getSample (0, frame));
    }

    check (sawMidFade, "the crossfade was observed");

    ContinuityResult result;
    double previousOutput = 0.0;

    for (int n = 0; n < static_cast<int> (output.size()); ++n)
    {
        const double phase = preRollPhase + static_cast<double> (n) * increment;
        const double sine = static_cast<double> (kAmplitude) * std::sin (phase);

        double gainA = 1.0;
        double gainB = 0.0;

        if (n >= kSwitchSample && n < kSwitchSample + kFadeSamples)
        {
            const double p = static_cast<double> (n - kSwitchSample)
                             / static_cast<double> (kFadeSamples);
            gainA = std::cos (p * juce::MathConstants<double>::halfPi);
            gainB = std::sin (p * juce::MathConstants<double>::halfPi);
        }
        else if (n >= kSwitchSample + kFadeSamples)
        {
            gainA = 0.0;
            gainB = 1.0;
        }

        const double expected = sine * (gainA + gainB);
        const double actual = output[static_cast<size_t> (n)];

        result.maxResidual = juce::jmax (result.maxResidual, std::abs (actual - expected));

        if (n > 0)
            result.maxOutputJump = juce::jmax (result.maxOutputJump,
                                               std::abs (actual - previousOutput));

        previousOutput = actual;
    }

    result.maxResidualDbfs = result.maxResidual > 0.0
        ? 20.0 * std::log10 (result.maxResidual)
        : -1000.0;

    return result;
}

}

void runContinuity()
{
    std::cout << "\n-- equal-power switching between two sine slots --" << std::endl;

    std::vector<float> output;
    bool producedEveryBlock = false;
    const auto result = runSwitchTest (output, producedEveryBlock);

    check (producedEveryBlock, "the switcher renders during and after the switch");

    std::cout << "      worst deviation from continuous reference: "
              << result.maxResidualDbfs << " dBFS" << std::endl;
    std::cout << "      largest output sample-to-sample jump: "
              << result.maxOutputJump << std::endl;

    check (result.maxResidual < 1.0e-3,
           "no discontinuity above -60 dBFS (residual < 1e-3)");
    check (result.maxResidualDbfs < -60.0,
           "worst deviation is below -60 dBFS");
}

}
