#include "Document.h"

#include "../TestSupport.h"
#include "Fixtures.h"

#include "controller/Mapping.h"

#include <string>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace LaunchpadMappingTest
{

using ControllerTest::check;

bool runDocumentChecks()
{
    Controller::SetMapping document;
    Controller::MappingError error;

    const bool loaded = Controller::loadSetMappingFile (IHUB_SET_YAML, document, error);

    check (loaded, std::string ("shared/set.yaml loads")
                       + (loaded ? std::string()
                                 : " (" + error.message + ":" + std::to_string (error.line) + ")"));

    if (! loaded)
        return false;

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

    return true;
}

} // namespace LaunchpadMappingTest
