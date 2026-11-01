// ihub-state-broadcast-test: the T103 acceptance check.
//
// The whole live-mirror path, without hardware:
//
//   LaunchpadDriver (the real note decoder and pad dispatch)
//     -> Api::StateBroadcast::publishPad()
//     -> /events WebSocket -> two connected clients
//
// Every event is timestamped on the test thread immediately before the note
// is injected, and again when a client receives the frame. The gap is the
// end-to-end latency the task contract caps at 100 ms.
//
// The same check is run for the UI path: `POST /arm` (what a grid tap does)
// must reach both sockets within 100 ms and carry the new armed slot.
//
// The MIDI leg uses the driver's injectMidiMessage() seam, so no ALSA
// sequencer port is opened and the test exercises the same decoder the MIDI
// thread uses.

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
