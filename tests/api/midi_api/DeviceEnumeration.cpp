#include "DeviceEnumeration.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"
#include "Fixtures.h"

#include "api/MidiBridge.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <iostream>

namespace ApiTest::MidiApi
{

void runDeviceEnumeration()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto devices = juce::MidiInput::getAvailableDevices();
    const auto inputs = Api::MidiBridge::enumerateInputs();

    check (inputs.size() == static_cast<std::size_t> (devices.size()),
           "the bridge lists every MIDI input JUCE reports");

    bool sawLaunchpad = false;
    bool sawKeystation = false;
    bool launchpadMi = false;
    bool keystationUsb = false;

    for (const auto& input : inputs)
    {
        check (! input.id.empty() && ! input.name.empty(),
               "every enumerated input has an id and a name");

        if (input.role == "launchpad")
        {
            sawLaunchpad = true;

            if (Api::MidiBridge::portForName (juce::String (input.name)) == "MI")
                launchpadMi = true;
        }

        if (input.role == "keystation")
        {
            sawKeystation = true;
            keystationUsb = Api::MidiBridge::portForName (juce::String (input.name)) == "USB";
        }
    }

    check (inputs.size() == 0 || sawLaunchpad || sawKeystation
               || std::none_of (inputs.begin(), inputs.end(),
                                [] (const Api::MidiInputState& input)
                                {
                                    return input.role != "input";
                                }),
           "unrecognised inputs are classified as plain MIDI inputs");

    std::cout << "midi-api-test: ALSA inputs=" << inputs.size()
              << " launchpad=" << (sawLaunchpad ? "yes" : "no")
              << " keystation=" << (sawKeystation ? "yes" : "no") << std::endl;

    if (sawLaunchpad)
        check (launchpadMi, "the Launchpad's MI port is identified");

    if (sawKeystation)
        check (keystationUsb, "the Keystation's USB port is identified");

    Api::MidiBridge bridge;
    const auto state = bridge.snapshot();

    check (state.inputs.size() == inputs.size(),
           "GET /midi mirrors the enumerated ALSA inputs");

    if (sawLaunchpad)
    {
        check (state.launchpad.port == "MI", "the idle bridge still names the MI port");
        check (! state.launchpad.connected, "the idle bridge reports no Launchpad driver");
    }

    if (sawKeystation)
    {
        check (state.keystation.port == "USB", "the idle bridge names the Keystation USB port");
        check (! state.keystation.connected, "the Keystation starts closed");
    }
}

} // namespace ApiTest::MidiApi
