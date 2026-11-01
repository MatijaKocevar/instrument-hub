// T126 acceptance test: MIDI inputs on the Audio page.
//
// Part 1 starts the control server against an in-memory MIDI backend and
// proves the request/response contract: GET /midi lists every ALSA-shaped
// input with its role and switch, POST /midi/inputs opens/closes one, and
// POST /midi/pad-test reports all 64 pads lit.
//
// Part 2 enumerates the real ALSA devices through the same JUCE calls the
// engine uses and compares the bridge's list with what JUCE reports. It then
// drives the pad test through a scripted LaunchpadDriver (message sink, no
// port) and checks the exact 64-note LED stream, its clear and the restore
// callback. No sound card or controller is required.

#include "TestSupport.h"
#include "midi_api/DeviceEnumeration.h"
#include "midi_api/PadTest.h"
#include "midi_api/Routes.h"

#include <iostream>

int main()
{
    ApiTest::MidiApi::runRoutes();
    ApiTest::MidiApi::runDeviceEnumeration();
    ApiTest::MidiApi::runPadTestStream();

    if (ApiTest::failures > 0)
    {
        std::cerr << "midi-api-test: " << ApiTest::failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "midi-api-test: all checks passed" << std::endl;
    return 0;
}
