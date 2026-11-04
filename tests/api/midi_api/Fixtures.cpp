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
    {
        input.port = name.find ("MI") != std::string::npos ? "MI" : "DA";
        input.leds = input.port == "MI";
        input.handshake = enabled && input.port == "MI";
    }
    else if (name.find ("USB") != std::string::npos)
    {
        input.port = "USB";
    }

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
                                      "keyboard", false));
    midi.inputs.push_back (makeInput ("ALSA:2:0",
                                      "Steinberg UR22mkII MIDI 1",
                                      "keyboard", false));

    midi.inputs[1].error = "the DA port carries no pads or LEDs; the grid uses the MI port";

    midi.padTest.durationMs = Api::MidiBridge::padTestDurationMs;
    return midi;
}

bool FakeBackend::setInput (const Api::MidiInputPatch& patch, std::string& error)
{
    const auto input = std::find_if (midi.inputs.begin(), midi.inputs.end(),
                                     [&patch] (const Api::MidiInputState& candidate)
                                     {
                                         return candidate.id == patch.id;
                                     });

    if (input == midi.inputs.end())
    {
        error = "unknown MIDI input: " + patch.id;
        return false;
    }

    if (patch.hasEnabled)
    {
        input->enabled = patch.enabled;
        input->open = patch.enabled;

        if (input->role == "launchpad")
            input->handshake = patch.enabled;
    }

    if (patch.hasRole)
    {
        if (patch.role != "keyboard" && patch.role != "grid")
        {
            error = "role must be \"keyboard\" or \"grid\"";
            return false;
        }

        input->role = patch.role;
        input->noteBase = -1;
        input->learnedNotes = 0;
        input->learnActive = false;
        input->error = patch.role == "grid"
                           ? "set a note base or learn all 64 pads before using this grid"
                           : "";
    }

    if (patch.hasNoteBase)
        input->noteBase = patch.noteBase;

    if (patch.hasLearn)
        input->learnActive = patch.learn;

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
        [this] (const Api::MidiInputPatch& patch, std::string& error)
        {
            return setInput (patch, error);
        },
        [this] (std::string& error)
        {
            return runPadTest (error);
        });
}

}
