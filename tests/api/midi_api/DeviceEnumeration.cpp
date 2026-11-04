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
    bool sawKeyboard = false;
    bool launchpadMi = false;

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
        else
        {
            sawKeyboard = true;
            check (input.role == "keyboard",
                   "every generic input is available as a keyboard");
        }
    }

    std::cout << "midi-api-test: ALSA inputs=" << inputs.size()
              << " launchpad=" << (sawLaunchpad ? "yes" : "no")
              << " keyboard=" << (sawKeyboard ? "yes" : "no") << std::endl;

    if (sawLaunchpad)
        check (launchpadMi, "the Launchpad's MI port is identified");

    Api::MidiBridge bridge;
    const auto state = bridge.snapshot();

    check (state.inputs.size() == inputs.size(),
           "GET /midi mirrors the enumerated ALSA inputs");

    const auto launchpad = std::find_if (state.inputs.begin(), state.inputs.end(),
                                         [] (const Api::MidiInputState& input)
                                         {
                                             return input.role == "launchpad"
                                                    && input.port == "MI";
                                         });

    if (launchpad != state.inputs.end())
    {
        check (! launchpad->open,
               "the idle bridge reports no Launchpad driver");
        check (launchpad->leds,
               "the Launchpad MI row advertises LED output");
    }

    const auto launchpadDa = std::find_if (state.inputs.begin(), state.inputs.end(),
                                           [] (const Api::MidiInputState& input)
                                           {
                                               return input.role == "launchpad"
                                                      && input.port == "DA";
                                           });

    if (launchpadDa != state.inputs.end())
        check (! launchpadDa->leds,
               "the Launchpad DA row advertises no LED output");
}

}
