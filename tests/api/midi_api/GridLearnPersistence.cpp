#include "GenericGrid.h"
#include "GenericGridFixtures.h"

#include "../TestSupport.h"

#include <utility>
#include <vector>

namespace ApiTest::MidiApi
{

using namespace GenericGridTest;

void runGridLearnPersistence()
{
    const auto directory = makeSettingsDirectory();
    const auto file = directory.getChildFile ("settings.json");

    ihub::settings::SettingsStore store (file);
    (void) store.load();

    auto backend = std::make_unique<FakeBackend> (makeDevices());
    Api::MidiBridge bridge (std::move (backend));
    bridge.setSettingsStore (&store);

    std::vector<std::pair<int, bool>> pads;
    bridge.setPadSink ([&pads] (int pad, bool pressed)
    {
        pads.emplace_back (pad, pressed);
        return true;
    });

    std::string error;

    check (bridge.updateInput (enablePatch ("10:0"), error),
           "the learn grid input can be enabled: " + error);
    check (bridge.updateInput (rolePatch ("10:0", "grid"), error),
           "the learn grid input can take the grid role: " + error);
    check (bridge.updateInput (learnPatch ("10:0", true), error),
           "learn mode starts: " + error);

    auto state = bridge.snapshot();
    const auto* grid = findInput (state, "10:0");

    check (grid != nullptr && grid->learnActive, "the snapshot reports the running learn");
    check (grid != nullptr && grid->learnedNotes == 0, "the learn starts at zero pads");

    pads.clear();

    for (int note = 40; note < 104; ++note)
        (void) bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (note));

    state = bridge.snapshot();
    grid = findInput (state, "10:0");

    check (pads.empty(), "learn presses never arm or switch a patch");
    check (grid != nullptr && ! grid->learnActive, "the learn stops after 64 distinct pads");
    check (grid != nullptr && grid->learnedNotes == 64, "the learned grid holds 64 pads");
    check (grid != nullptr && grid->noteBase == -1,
           "learning replaces the note base with the learned map");
    check (grid != nullptr && grid->error.empty(), "a learned grid has no error");

    pads.clear();
    (void) bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (40));
    (void) bridge.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (103));
    check (pads.size() == 2 && pads[0].first == 0 && pads[1].first == 63,
           "the learned order maps the first and last notes to their pads");

    check (file.loadFileAsString().contains ("gridInputs"),
           "the grid role reaches the settings file");
    check (file.loadFileAsString().contains ("\"notes\""),
           "the learned note map reaches the settings file");

    ihub::settings::SettingsStore reloaded (file);
    check (reloaded.load() == ihub::settings::LoadStatus::loaded,
           "the grid settings reload from disk");

    auto restoredBackend = std::make_unique<FakeBackend> (makeDevices());
    Api::MidiBridge restored (std::move (restoredBackend));
    restored.setSettingsStore (&reloaded);

    std::vector<std::pair<int, bool>> restoredPads;
    restored.setPadSink ([&restoredPads] (int pad, bool pressed)
    {
        restoredPads.emplace_back (pad, pressed);
        return true;
    });

    const auto restoredState = restored.snapshot();
    const auto* restoredGrid = findInput (restoredState, "10:0");

    check (restoredGrid != nullptr && restoredGrid->role == "grid",
           "the restored input still carries the grid role");
    check (restoredGrid != nullptr && restoredGrid->learnedNotes == 64,
           "the restored input keeps its learned note map");

    check (restored.processMidiMessage ("10:0", "Nano PAD USB MIDI", noteOn (40)),
           "a restored learned note is accepted");
    check (restoredPads.size() == 1 && restoredPads[0].first == 0,
           "the restored learned note maps to the same pad");

    directory.deleteRecursively();
}

}
