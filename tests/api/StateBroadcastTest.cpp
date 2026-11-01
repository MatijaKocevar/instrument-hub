#include "TestSupport.h"
#include "state_broadcast/Accounting.h"
#include "state_broadcast/InitialState.h"
#include "state_broadcast/PadPath.h"
#include "state_broadcast/UiPath.h"

#include <juce_events/juce_events.h>

#include <exception>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    try
    {
        ApiTest::StateBroadcast::Rig rig;

        if (! rig.start())
            return 1;

        ApiTest::StateBroadcast::runInitialState (rig);
        ApiTest::StateBroadcast::runPadPath (rig);
        ApiTest::StateBroadcast::runUiPath (rig);
        ApiTest::StateBroadcast::runAccounting (rig);

        rig.stop();

        std::cout << (ApiTest::failures == 0 ? "PASS" : "FAIL")
                  << ": state broadcast acceptance (" << ApiTest::failures
                  << " failure(s))" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    return ApiTest::failures == 0 ? 0 : 1;
}
