#include "TestSupport.h"
#include "reverb/Damping.h"
#include "reverb/Decay.h"
#include "reverb/DryPath.h"
#include "reverb/PreDelay.h"
#include "reverb/Realtime.h"
#include "reverb/Stability.h"
#include "reverb/State.h"
#include "reverb/Width.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DspTest::Reverb::runDryPath();
    DspTest::Reverb::runDecay();
    DspTest::Reverb::runDamping();
    DspTest::Reverb::runPreDelay();
    DspTest::Reverb::runWidth();
    DspTest::Reverb::runStability();
    DspTest::Reverb::runRealtime();
    DspTest::Reverb::runState();

    std::cout << (DspTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << DspTest::failures << " failed check(s)" << std::endl;

    return DspTest::failures == 0 ? 0 : 1;
}
