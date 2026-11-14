#include "MappingImport.h"

#include "PatchColours.h"
#include "ProjectLimits.h"

#include <string>
#include <utility>

namespace ihub::app
{

    void importMappingPatches (const Controller::SetMapping& document,
                               std::map<int, project::SlotConfig>& slots)
    {
        for (const auto& track : document.tracks)
        {
            if (track.slot < 0 || track.slot >= maximumProjectSlots)
                continue;

            auto& slot = slots[track.slot];
            slot = project::SlotConfig{};
            slot.index = track.slot;
            slot.active = track.slot == 0;
            slot.name = track.name;
            slot.colorIndex = 14;
            slot.colour = colourForIndex (14);
            slot.instrument.kind = project::SourceKind::empty;
            slot.instrument.id = track.id;
            slot.instrument.name.clear ();
            slot.instrument.gain = slot.gain;
        }
    }

}
