#include "Handshake.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"

namespace DriverTest
{

using ControllerTest::check;

void runHandshakeChecks (Rig& rig)
{
    const auto programmer = Launchpad::programmerModeFrame (true);
    rig.driver.injectMidiMessage (juce::MidiMessage (programmer.bytes.data(),
                                                     static_cast<int> (programmer.bytes.size())));

    check (rig.driver.hasReceivedHandshakeEcho(),
           "an injected programmer-mode echo is recognised");

    rig.sent.clear();
}

}
