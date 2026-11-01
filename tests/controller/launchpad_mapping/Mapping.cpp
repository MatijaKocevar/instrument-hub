#include "Mapping.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"
#include "controller/Mapping.h"

#include <string>
#include <utility>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace LaunchpadMappingTest
{

using ControllerTest::check;

void runMappingChecks()
{
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
}

}
