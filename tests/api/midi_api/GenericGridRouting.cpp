#include "GenericGrid.h"
#include "GenericGridFixtures.h"

#include "../TestSupport.h"

#include <string>
#include <utility>
#include <vector>

namespace ApiTest::MidiApi
{

using namespace GenericGridTest;

void runGenericGridRouting()
{
    auto backend = std::make_unique<FakeBackend> (makeDevices());
    Api::MidiBridge bridge (std::move (backend));

    std::vector<juce::MidiMessage> forwarded;
    std::vector<std::pair<int, bool>> pads;

    bridge.setMidiSink ([&forwarded] (const juce::MidiMessage& message)
    {
        forwarded.push_back (message);
        return true;
    });
    bridge.setPadSink ([&pads] (int pad, bool pressed)
    {
        pads.emplace_back (pad, pressed);
        return true;
    });

    std::string error;

    check (bridge.processMidiMessage ("10:1", "Alesis Q49 USB MIDI", noteOn (60)),
           "a generic keyboard note is accepted");
    check (forwarded.size() == 1,
           "a generic keyboard note reaches the instrument queue");
    check (pads.empty(), "a generic keyboard note does not drive the grid");

    check (! bridge.processMidiMessage ("10:2", "Launchpad Mini MK3 LPMiniMK3 MI",
                                        noteOn (11)),
           "a Launchpad note is never forwarded as a keyboard note");
    check (forwarded.size() == 1, "the Launchpad note stays on the driver path");

    check (bridge.updateInput (enablePatch ("10:0"), error),
           "the generic pad controller can be enabled: " + error);
    check (bridge.updateInput (rolePatch ("10:0", "grid"), error),
           "the generic pad controller can take the grid role: " + error);

    auto state = bridge.snapshot();
    const auto* grid = findInput (state, "10:0");

    check (grid != nullptr && grid->role == "grid", "the input reports the grid role");
    check (grid != nullptr && grid->leds == false,
           "a generic grid reports no LED output");
    check (grid != nullptr && ! grid->error.empty(),
           "an unconfigured grid explains what is missing");

    check (! bridge.updateInput (noteBasePatch ("10:0", 99), error),
           "a note base past 64 is rejected");
    check (error.find ("noteBase") != std::string::npos,
           "the note-base rejection is readable");

    check (bridge.updateInput (noteBasePatch ("10:0", 36), error),
           "the generic grid accepts a note base: " + error);

    pads.clear();
    forwarded.clear();

    check (bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (36)),
           "the first note of a note-base grid is consumed by the grid");
    check (forwarded.empty(), "the grid note is not forwarded to the instrument");
    check (pads.size() == 1 && pads[0] == std::pair<int, bool> { 0, true },
           "note base 36 maps to the bottom-left pad");

    check (bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOff (36)),
           "the first pad release is consumed by the grid");
    check (pads.size() == 2 && pads[1] == std::pair<int, bool> { 0, false },
           "the pad release reaches the same pad");

    check (bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (99)),
           "the last note of a note-base grid is consumed by the grid");
    check (pads.size() == 3 && pads[2] == std::pair<int, bool> { 63, true },
           "note base 36 + 63 maps to the top-right pad");

    check (bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (35)),
           "a note outside the note base is consumed by the grid");
    check (pads.size() == 3, "a note outside the note base does not pick a pad");

    check (! bridge.updateInput (rolePatch ("10:2", "grid"), error),
           "a Launchpad cannot become a generic grid");
    check (error.find ("own grid protocol") != std::string::npos,
           "the Launchpad grid rejection is readable");

    check (! bridge.updateInput (noteBasePatch ("10:1", 36), error),
           "a note base needs the grid role first");
    check (error.find ("grid role") != std::string::npos,
           "the missing-role rejection is readable");

    check (! bridge.updateInput (noteBasePatch ("99:0", 36), error),
           "an unknown input is rejected");
    check (error.find ("unknown MIDI input") != std::string::npos,
           "the unknown-input rejection is readable");

    check (! bridge.updateInput (learnPatch ("10:2", true), error),
           "a Launchpad cannot start a generic learn");
    check (error.find ("own grid protocol") != std::string::npos,
           "the Launchpad learn rejection is readable");
}

}
