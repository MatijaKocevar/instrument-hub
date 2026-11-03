#include "TestSupport.h"

#include "settings/MidiInputNames.h"

namespace SettingsTest
{

void runMidiInputNameChecks()
{
    using ihub::settings::MidiInput;

    const MidiInput keys { "Keystation Mini 32 MK3", "Keystation Mini 32 MK3 USB Audio" };
    const MidiInput other { "Keystation Mini 32 MK3", "Keystation Mini 32 MK3 MIDI 1" };
    const MidiInput wrongClient { "Another client", "Keystation Mini 32 MK3 USB Audio" };
    const MidiInput noClient { "", "Keystation Mini 32 MK3 USB Audio" };
    const MidiInput unknownPort { "", "Unknown MIDI 9" };

    check (ihub::settings::sameMidiInput (keys, keys),
           "a stored MIDI input matches itself");
    check (! ihub::settings::sameMidiInput (keys, other),
           "different ports on the same client are different inputs");
    check (! ihub::settings::sameMidiInput (keys, wrongClient),
           "the same port name under a different client is a different input");
    check (ihub::settings::sameMidiInput (keys, noClient),
           "a missing client name falls back to the port name");

    std::vector<MidiInput> inputs;
    ihub::settings::addMidiInput (inputs, keys);
    ihub::settings::addMidiInput (inputs, keys);
    check (inputs.size() == 1, "adding the same pair twice keeps one entry");

    ihub::settings::addMidiInput (inputs, other);
    ihub::settings::addMidiInput (inputs, wrongClient);
    check (inputs.size() == 3, "each distinct client/port pair is kept");

    check (ihub::settings::containsMidiInput (inputs, keys),
           "contains finds a stored pair");
    check (! ihub::settings::containsMidiInput (inputs, unknownPort),
           "contains rejects a port that is not stored");

    check (ihub::settings::removeMidiInput (inputs, keys),
           "removing a stored pair reports a change");
    check (inputs.size() == 2 && ! ihub::settings::containsMidiInput (inputs, keys),
           "removing a pair drops only that pair");
    check (! ihub::settings::removeMidiInput (inputs, keys),
           "removing a pair that is gone reports no change");
    check (inputs[0].port == "Keystation Mini 32 MK3 MIDI 1"
               && inputs[1].client == "Another client",
           "the remaining pairs are untouched");
}

}
