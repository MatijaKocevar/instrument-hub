#include "TestSupport.h"
#include "delay/Acceptance.h"
#include "delay/Bypass.h"
#include "delay/Feedback.h"
#include "delay/FreeTime.h"
#include "delay/Realtime.h"
#include "delay/State.h"
#include "delay/SyncTable.h"
#include "delay/Transport.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DspTest::Delay::runAcceptance();
    DspTest::Delay::runSyncTable();
    DspTest::Delay::runFreeTime();
    DspTest::Delay::runFeedback();
    DspTest::Delay::runTransport();
    DspTest::Delay::runBypass();
    DspTest::Delay::runRealtime();
    DspTest::Delay::runState();

    std::cout << (DspTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << DspTest::failures << " failed check(s)" << std::endl;

    return DspTest::failures == 0 ? 0 : 1;
}
