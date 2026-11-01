#include "Fixtures.h"

#include <algorithm>

namespace ApiTest::MidiApi
{

Api::MidiInputState makeInput (const std::string& id,
                               const std::string& name,
                               const std::string& role,
                               bool enabled)
{
    Api::MidiInputState input;
    input.id = id;
    input.name = name;
    input.role = role;
    input.enabled = enabled;
    input.open = enabled;

    if (role == "launchpad")
        input.port = name.find ("MI") != std::string::npos ? "MI" : "DA";
    else if (role == "keystation")
        input.port = "USB";

    return input;
}

Api::MidiStateSnapshot makeFakeMidi()
{
    Api::MidiStateSnapshot midi;

    midi.inputs.push_back (makeInput ("ALSA:4:1",
                                      "Launchpad Mini MK3 LPMiniMK3 MI",
                                      "launchpad", true));
    midi.inputs.push_back (makeInput ("ALSA:4:0",
                                      "Launchpad Mini MK3 LPMiniMK3 DA",
                                      "launchpad", false));
    midi.inputs.push_back (makeInput ("ALSA:5:0",
                                      "Keystation Mini 32 MK3 USB Audio",
                                      "keystation", false));
    midi.inputs.push_back (makeInput ("ALSA:2:0",
                                      "Steinberg UR22mkII MIDI 1",
                                      "input", false));

    midi.launchpad.name = "Launchpad Mini MK3 LPMiniMK3 MI";
    midi.launchpad.port = "MI";
    midi.launchpad.connected = true;
    midi.launchpad.handshake = true;
    midi.launchpad.inputName = midi.launchpad.name;
    midi.launchpad.outputName = midi.launchpad.name;

    midi.keystation.name = "Keystation Mini 32 MK3 USB Audio";
    midi.keystation.port = "USB";

    midi.padTest.durationMs = Api::MidiBridge::padTestDurationMs;
    return midi;
}

bool FakeBackend::setInput (const std::string& id, bool enabled, std::string& error)
{
    const auto input = std::find_if (midi.inputs.begin(), midi.inputs.end(),
                                     [&id] (const Api::MidiInputState& candidate)
                                     {
                                         return candidate.id == id;
                                     });

    if (input == midi.inputs.end())
    {
        error = "unknown MIDI input: " + id;
        return false;
    }

    input->enabled = enabled;
    input->open = enabled;

    if (input->role == "launchpad")
        midi.launchpad.connected = enabled;

    if (input->role == "keystation")
        midi.keystation.connected = enabled;

    return true;
}

bool FakeBackend::runPadTest (std::string&)
{
    ++padTests;
    midi.padTest.active = true;
    midi.padTest.lit = 64;
    return true;
}

void FakeBackend::attach (Api::Server& server)
{
    server.setMidiHandlers (
        [this] { return midi; },
        [this] (const std::string& id, bool enabled, std::string& error)
        {
            return setInput (id, enabled, error);
        },
        [this] (std::string& error)
        {
            return runPadTest (error);
        });
}

}
