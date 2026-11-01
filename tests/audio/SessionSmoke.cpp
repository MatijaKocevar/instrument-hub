#include "TestSupport.h"
#include "session_smoke/Composition.h"
#include "session_smoke/Fixtures.h"
#include "session_smoke/HardSwitch.h"
#include "session_smoke/Switch.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-session-smoke-test\n"
              << "  slots: " << AudioTest::SessionSmoke::kNumSlots
              << ", block: " << AudioTest::SessionSmoke::kBlockSize
              << ", sample rate: " << AudioTest::SessionSmoke::kSampleRate << std::endl;

    AudioTest::SessionSmoke::runComposition();
    AudioTest::SessionSmoke::runSwitch();
    AudioTest::SessionSmoke::runHardSwitch();

    std::cout << (AudioTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << AudioTest::failures << " failed check(s)" << std::endl;

    return AudioTest::failures == 0 ? 0 : 1;
}
