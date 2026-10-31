// ihub-launchpad-mapping-test: the T116 64-pad mapping acceptance check.
//
// It is pure C++ (no JUCE, no MIDI port) and proves:
//
//   1. shared/set.yaml binds all 64 grid pads: eight families of eight patches,
//      row-major from the bottom-left, and every pad resolves to its own slot.
//   2. Every pad press arms exactly the slot named by its binding.
//   3. The Launchpad's top row selects the eight macro pages and the right
//      column selects the eight scenes declared in the same document.
//   4. Malformed bank/scene/pad entries are rejected instead of half-applied.

#include "controller/Mapping.h"
#include "controller/LaunchpadProtocol.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

const std::vector<std::string> expectedFamilies {
    "keys", "drums", "bass", "pads", "lead", "fx", "perc", "vox"
};

std::vector<std::string> expectedInstruments (const std::string& family)
{
    if (family == "keys")
        return { "piano", "strings", "rhodes", "organ", "harp", "choir", "celesta", "clav" };
    if (family == "drums")
        return { "kit", "snare", "hihats", "toms", "clap", "rimshot", "cymbals", "kick" };
    if (family == "bass")
        return { "sub", "finger", "pick", "fretless", "synth", "upright", "growl", "octave" };
    if (family == "pads")
        return { "air", "warm", "glass", "choir", "motion", "dark", "shimmer", "tape" };
    if (family == "lead")
        return { "saw", "square", "sine", "supersaw", "pluck", "brass", "flute", "whistle" };
    if (family == "fx")
        return { "riser", "downlifter", "impact", "sweep", "noise", "glitch", "drone", "reverse" };
    if (family == "perc")
        return { "shaker", "tambourine", "conga", "bongo", "triangle", "woodblock", "cowbell", "tabla" };
    if (family == "vox")
        return { "ah", "oo", "eh", "whisper", "shout", "vocoder", "breath", "choir" };

    return {};
}

} // namespace

int main()
{
    std::cout << "ihub-launchpad-mapping-test\n";

    Controller::SetMapping document;
    Controller::MappingError error;

    const bool loaded = Controller::loadSetMappingFile (IHUB_SET_YAML, document, error);

    check (loaded, std::string ("shared/set.yaml loads")
                       + (loaded ? std::string()
                                 : " (" + error.message + ":" + std::to_string (error.line) + ")"));

    if (! loaded)
        return 1;

    //==========================================================================
    // 1. Eight families x eight patches
    //==========================================================================
    check (document.families.size() == 8,
           "set.yaml declares eight families (got "
               + std::to_string (document.families.size()) + ")");

    for (const auto& familyId : expectedFamilies)
    {
        const auto family = document.families.find (familyId);
        check (family != document.families.end(),
               "family '" + familyId + "' is declared");

        if (family == document.families.end())
            continue;

        check (family->second.instruments.size() == 8,
               "family '" + familyId + "' has eight patches (got "
                   + std::to_string (family->second.instruments.size()) + ")");
    }

    check (document.countPads() == 64,
           "set.yaml binds all 64 grid pads (got "
               + std::to_string (document.countPads()) + ")");
    check (document.countEncoders() == 8, "set.yaml declares eight macros");
    check (document.countBanks() == 8, "set.yaml declares eight macro pages");
    check (document.countScenes() == 8, "set.yaml declares eight scenes");

    //==========================================================================
    // 2. Every pad resolves to the expected family, patch and slot
    //==========================================================================
    bool allPadsResolve = true;
    bool allSlotsUnique = true;
    std::vector<bool> slotSeen (Controller::SetMapping::maximumSlot + 1, false);

    for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
    {
        const auto* binding = document.padBinding (pad);

        if (binding == nullptr)
        {
            check (false, "pad " + std::to_string (pad) + " has a binding");
            allPadsResolve = false;
            continue;
        }

        const int row = pad / 8;
        const int column = pad % 8;
        const auto& familyId = expectedFamilies[static_cast<std::size_t> (row)];
        const auto instruments = expectedInstruments (familyId);

        const bool matches = binding->pad == pad
                             && binding->family == familyId
                             && binding->instrument == instruments[static_cast<std::size_t> (column)]
                             && binding->slot == pad;

        if (! matches)
            check (false, "pad " + std::to_string (pad) + " resolves to "
                              + binding->family + "/" + binding->instrument
                              + " on slot " + std::to_string (binding->slot));

        allPadsResolve = allPadsResolve && matches;

        const auto* instrument = document.instrumentFor (*binding);

        if (instrument == nullptr || instrument->slot != pad)
            allPadsResolve = false;

        if (binding->slot >= 0 && binding->slot <= Controller::SetMapping::maximumSlot)
        {
            if (slotSeen[static_cast<std::size_t> (binding->slot)])
                allSlotsUnique = false;

            slotSeen[static_cast<std::size_t> (binding->slot)] = true;
        }
        else
        {
            allSlotsUnique = false;
        }
    }

    check (allPadsResolve, "all 64 pads resolve to their family/patch/slot");
    check (allSlotsUnique, "the 64 pads use 64 distinct slots (0..63)");

    //==========================================================================
    // 3. Pad presses arm the slot of their binding
    //==========================================================================
    Controller::Mapping mapping;
    std::vector<int> armedSlots;
    std::vector<std::pair<int, std::string>> bankEvents;
    std::vector<std::pair<int, std::string>> sceneEvents;

    Controller::Mapping::Listeners listeners;

    listeners.armSlot = [&] (int slot) { armedSlots.push_back (slot); };

    listeners.bankEvent = [&] (int bank, const Controller::MacroBankSpec* page)
    {
        bankEvents.emplace_back (bank, page != nullptr ? page->name : std::string());
    };

    listeners.sceneEvent = [&] (int scene, const Controller::SceneSpec* selection)
    {
        sceneEvents.emplace_back (scene, selection != nullptr ? selection->family : std::string());
    };

    mapping.setListeners (listeners);

    Controller::MappingError loadError;
    check (mapping.load (IHUB_SET_YAML, &loadError),
           "Mapping loads shared/set.yaml" + (loadError.isEmpty()
                                                  ? std::string()
                                                  : " (" + loadError.message + ")"));

    bool everyPressArms = true;

    for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
    {
        const bool handled = mapping.handlePad (pad, true);

        if (! handled || mapping.getArmedSlot() != pad)
        {
            check (false, "pad " + std::to_string (pad) + " arms slot "
                              + std::to_string (pad) + " (got "
                              + std::to_string (mapping.getArmedSlot()) + ")");
            everyPressArms = false;
        }

        // Release must not change the selection.
        const bool released = mapping.handlePad (pad, false);

        if (released)
        {
            check (false, "pad " + std::to_string (pad) + " release is not a selection");
            everyPressArms = false;
        }
    }

    check (everyPressArms, "a press on each of the 64 pads arms its own slot");
    check (static_cast<int> (armedSlots.size()) == 64,
           "the arm listener saw 64 presses (got "
               + std::to_string (armedSlots.size()) + ")");

    bool armOrderCorrect = static_cast<int> (armedSlots.size()) == 64;

    for (int pad = 0; pad < 64 && armOrderCorrect; ++pad)
        armOrderCorrect = armedSlots[static_cast<std::size_t> (pad)] == pad;

    check (armOrderCorrect, "the armed slot sequence is 0..63");

    //==========================================================================
    // 4. Top row -> macro pages, right column -> scenes
    //==========================================================================
    bool everyBank = true;

    for (int index = 0; index < Controller::SetMapping::maximumBanks; ++index)
    {
        const int cc = Launchpad::topRowCCForIndex (index);
        const bool handled = mapping.handleButton (cc, true);

        everyBank = everyBank && handled
                    && mapping.getActiveBank() == index
                    && mapping.handleButton (cc, false) == false;
    }

    check (everyBank, "CC 91..98 select macro pages 0..7");
    check (static_cast<int> (bankEvents.size()) == 8,
           "each top-row press reported a macro page (got "
               + std::to_string (bankEvents.size()) + ")");
    check (! bankEvents.empty() && bankEvents.front().second == "Filter"
               && bankEvents.back().second == "Pan",
           "top-row page names come from set.yaml");

    bool everyScene = true;

    for (int index = 0; index < Controller::SetMapping::maximumScenes; ++index)
    {
        const int cc = Launchpad::rightColumnCCForIndex (index);
        const bool handled = mapping.handleButton (cc, true);

        everyScene = everyScene && handled
                     && mapping.getActiveScene() == index
                     && mapping.handleButton (cc, false) == false;
    }

    check (everyScene, "CC 89..19 select scenes 0..7");
    check (sceneEvents.size() == 8,
           "each right-column press reported a scene (got "
               + std::to_string (sceneEvents.size()) + ")");
    check (! sceneEvents.empty() && sceneEvents.front().second == "vox"
               && sceneEvents.back().second == "keys",
           "scene 0 is the top row (vox) and scene 7 the bottom row (keys)");

    check (! mapping.handleButton (Launchpad::logoCC, true),
           "the logo button is not a bank or scene");
    check (! mapping.handleButton (99, true), "an unknown CC is ignored");

    //==========================================================================
    // 5. Malformed control rows are rejected
    //==========================================================================
    auto expectInvalid = [] (const std::string& label,
                             const std::string& yaml,
                             const std::string& fragment)
    {
        Controller::SetMapping parsed;
        Controller::MappingError parseError;
        const bool ok = Controller::parseSetMappingYaml (yaml, parsed, parseError);

        check (! ok, label + " is rejected");
        check (! ok && parseError.message.find (fragment) != std::string::npos,
               label + " reports '" + fragment + "' (got '"
                   + parseError.message + "')");
    };

    const std::string prefix = R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros:
  cutoff:
    name: Cutoff
    parameter: filter.cutoff
pads:
  - pad: 0
    family: keys
    instrument: piano
encoders: []
)";

    expectInvalid ("a duplicate bank", prefix + R"(banks:
  - bank: 0
    name: A
    macros:
      - cutoff
  - bank: 0
    name: B
    macros:
      - cutoff
scenes: []
)", "duplicate bank");

    expectInvalid ("a bank with an unknown macro", prefix + R"(banks:
  - bank: 0
    name: A
    macros:
      - missing
scenes: []
)", "unknown macro");

    expectInvalid ("a duplicate scene", prefix + R"(banks: []
scenes:
  - scene: 0
    name: A
    family: keys
  - scene: 0
    name: B
    family: keys
)", "duplicate scene");

    expectInvalid ("a scene with an unknown family", prefix + R"(banks: []
scenes:
  - scene: 0
    name: A
    family: drums
)", "unknown family");

    expectInvalid ("pad 64", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros:
  cutoff:
    name: Cutoff
    parameter: filter.cutoff
pads:
  - pad: 64
    family: keys
    instrument: piano
encoders: []
banks: []
scenes: []
)", "pad must be within");

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
