#include "Check.h"
#include "pad_audio/Constants.h"
#include "pad_audio/MappingScenario.h"

#include "controller/LaunchpadProtocol.h"
#include "controller/Mapping.h"

#include <filesystem>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace PadAudio
{
using E2E::check;


void runMappingScenario()
{
    Controller::Mapping mapping;
    Controller::MappingError error;

    const bool loaded = mapping.load (std::filesystem::path (IHUB_SET_YAML), &error);
    check (loaded, "shared/set.yaml loads"
                       + (loaded ? std::string() : ": " + error.message));
    check (mapping.getMapping() != nullptr, "the mapping was published");

    if (loaded)
    {
        int bound = 0;

        for (int pad = 0; pad < Launchpad::padCount; ++pad)
        {
            const auto binding = mapping.getPadBinding (pad);

            if (binding.has_value() && binding->slot == pad)
                ++bound;
        }

        check (bound == Launchpad::padCount,
               "all 64 grid pads resolve to their own binding ("
                   + std::to_string (bound) + "/64)");
        check (bound >= kDefaultSlots,
               "the first eight pads address the eight composed slots");
    }
}

}
