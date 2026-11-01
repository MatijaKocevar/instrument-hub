#include "Fixtures.h"
#include "TestSupport.h"
#include "voices/Choke.h"
#include "voices/Gate.h"
#include "voices/Pool.h"
#include "voices/Realtime.h"
#include "voices/Setup.h"
#include "voices/Stealing.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VoiceTest::Rig rig;
    rig.directory = SamplerTest::testDirectory ("sampler-instrument-fixtures");

    std::cout << "ihub-sampler-instrument-test\n"
              << "fixtures: " << rig.directory.getFullPathName() << "\n"
              << std::endl;

    VoiceTest::runSetupChecks (rig);
    VoiceTest::runPoolChecks (rig);
    VoiceTest::runChokeChecks (rig);
    VoiceTest::runGateChecks (rig);
    VoiceTest::runStealingChecks (rig);
    VoiceTest::runRealtimeChecks (rig);

    std::cout << std::endl;

    if (SamplerTest::failures == 0)
    {
        std::cout << "PASS: pad triggering and choke (T092)" << std::endl;
        return 0;
    }

    std::cout << "FAIL: " << SamplerTest::failures << " check(s) failed" << std::endl;
    return 1;
}
