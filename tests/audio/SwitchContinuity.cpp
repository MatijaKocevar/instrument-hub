#include "TestSupport.h"
#include "switch_continuity/Cases.h"
#include "switch_continuity/Constants.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-switch-continuity-test\n"
              << "  source slots: " << AudioTest::SwitchContinuity::kNumSlots
              << ", switches: " << AudioTest::SwitchContinuity::kSwitchCount
              << ", block: " << AudioTest::SwitchContinuity::kBlockSize
              << ", sample rate: " << AudioTest::SwitchContinuity::kSampleRate
              << ", seed: " << AudioTest::SwitchContinuity::kSeed << std::endl;

    AudioTest::SwitchContinuity::runCase ("ramped switching",
                                          AudioTest::SwitchContinuity::kFadeMilliseconds,
                                          false);

    AudioTest::SwitchContinuity::runCase ("hard switching (ramp disabled)", 0.0f, true);

    std::cout << (AudioTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << AudioTest::failures << " failed check(s)" << std::endl;

    return AudioTest::failures == 0 ? 0 : 1;
}
