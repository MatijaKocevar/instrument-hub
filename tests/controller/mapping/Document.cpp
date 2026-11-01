#include "Document.h"

#include "../TestSupport.h"

#include "controller/Mapping.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

#ifndef IHUB_SET_SCHEMA
 #define IHUB_SET_SCHEMA "shared/schema/set.schema.json"
#endif

namespace MappingTest
{

using ControllerTest::check;

void runDocumentChecks()
{
    Controller::SetMapping shipped;
    Controller::MappingError error;

    const bool loaded = Controller::loadSetMappingFile (IHUB_SET_YAML, shipped, error);

    check (loaded, std::string ("shared/set.yaml loads")
                       + (loaded ? std::string()
                                 : " (" + error.message + ":" + std::to_string (error.line) + ")"));

    if (loaded)
    {
        check (shipped.schemaVersion == 1, "set.yaml declares schemaVersion 1");
        check (! shipped.name.empty(), "set.yaml has a set name");
        check (shipped.families.size() >= 5,
               "set.yaml declares at least five families (got "
                   + std::to_string (shipped.families.size()) + ")");
        check (shipped.macros.size() == 8,
               "set.yaml declares eight macros (got "
                   + std::to_string (shipped.macros.size()) + ")");
        check (shipped.countPads() >= 8,
               "set.yaml binds at least eight pads (got "
                   + std::to_string (shipped.countPads()) + ")");
        check (shipped.countEncoders() == 8,
               "set.yaml binds all eight encoders (got "
                   + std::to_string (shipped.countEncoders()) + ")");
        check (shipped.countPatches() == 64,
               "set.yaml declares 64 patches (got "
                   + std::to_string (shipped.countPatches()) + ")");

        const auto keysFamily = shipped.families.find ("keys");
        check (keysFamily != shipped.families.end()
                   && keysFamily->second.colorIndex == 0,
               "the keys family declares its colour default");

        const auto* pianoPatch = keysFamily != shipped.families.end()
                                     ? keysFamily->second.findInstrument ("piano")
                                     : nullptr;
        const auto* stringsPatch = keysFamily != shipped.families.end()
                                       ? keysFamily->second.findInstrument ("strings")
                                       : nullptr;

        check (pianoPatch != nullptr
                   && pianoPatch->colorIndex == 1
                   && pianoPatch->resolvedColorIndex == 1,
               "keys/piano overrides the family colour");
        check (stringsPatch != nullptr
                   && stringsPatch->colorIndex == -1
                   && stringsPatch->resolvedColorIndex == 0,
               "keys/strings inherits the family colour");
        check (pianoPatch != nullptr
                   && std::abs (pianoPatch->volumeDb - (-2.5f)) <= 1.0e-5f
                   && std::abs (pianoPatch->pan - (-0.15f)) <= 1.0e-5f
                   && ! pianoPatch->muted && ! pianoPatch->solo,
               "keys/piano carries volume, pan, mute and solo");
        check (pianoPatch != nullptr && pianoPatch->fxChain.size() == 1
                   && pianoPatch->fxChain.front().id == "eq",
               "keys/piano carries its FX chain");

        const auto* pianoPad = shipped.padBinding (0);
        check (pianoPad != nullptr && shipped.colorIndexFor (*pianoPad) == 1,
               "the resolved colour reaches the pad binding");

        const auto* padZero = shipped.padBinding (0);
        check (padZero != nullptr && padZero->family == "keys"
                   && padZero->instrument == "piano" && padZero->slot == 0,
               "pad 0 resolves to keys/piano on slot 0");

        const auto* encoderZero = shipped.encoderBinding (0);
        const auto* cutoff = encoderZero != nullptr ? shipped.macroFor (*encoderZero) : nullptr;

        check (cutoff != nullptr && cutoff->id == "cutoff"
                   && cutoff->parameter == "filter.cutoff",
               "encoder 0 resolves to the cutoff macro");

        bool allSlotsValid = true;

        for (const auto* instrument : shipped.allInstruments())
            if (instrument->slot < 0 || instrument->slot > Controller::SetMapping::maximumSlot)
                allSlotsValid = false;

        check (allSlotsValid, "every instrument declares a valid slot");
    }

    std::ifstream schemaFile (IHUB_SET_SCHEMA);
    std::string schemaText ((std::istreambuf_iterator<char> (schemaFile)),
                            std::istreambuf_iterator<char>());

    check (! schemaText.empty() && schemaText.find ("\"schemaVersion\"") != std::string::npos,
           "shared/schema/set.schema.json exists and describes schemaVersion");
}

} // namespace MappingTest
