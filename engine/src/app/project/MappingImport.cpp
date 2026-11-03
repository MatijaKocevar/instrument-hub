#include "MappingImport.h"

#include "ProjectLimits.h"

#include <array>
#include <string>
#include <utility>

namespace ihub::app
{

namespace
{

constexpr std::array<const char*, 16> patchColours {
    "#fb923c", "#facc15", "#a3e635", "#4ade80",
    "#34d399", "#2dd4bf", "#22d3ee", "#38bdf8",
    "#60a5fa", "#818cf8", "#a78bfa", "#c084fc",
    "#e879f9", "#f472b6", "#94a3b8", "#e2e8f0"
};

int clampColourIndex (int index) noexcept
{
    return juce::jlimit (0, 15, index);
}

std::string colourForIndex (int index)
{
    return patchColours[static_cast<std::size_t> (clampColourIndex (index))];
}

void importInstrument (const Controller::InstrumentSpec& instrument,
                       project::SlotConfig& slot)
{
    slot.name = instrument.name.empty() ? instrument.id : instrument.name;
    slot.setVolumeDb (instrument.volumeDb);
    slot.pan = instrument.pan;
    slot.muted = instrument.muted;
    slot.solo = instrument.solo;

    const int colourIndex = clampColourIndex (instrument.resolvedColorIndex);
    slot.colorIndex = colourIndex;
    slot.colour = colourForIndex (colourIndex);

    if (! project::sourceKindFromString (instrument.kind, slot.instrument.kind))
        slot.instrument.kind = project::SourceKind::empty;

    slot.instrument.id = instrument.id;
    slot.instrument.name = instrument.name;
    slot.instrument.pluginFormat = instrument.pluginFormat.empty()
                                       ? "VST3"
                                       : instrument.pluginFormat;
    slot.instrument.pluginId = instrument.pluginId;
    slot.instrument.pluginName = instrument.pluginName;
    slot.instrument.pluginManufacturer = instrument.pluginManufacturer;
    slot.instrument.pluginVersion = instrument.pluginVersion;
    slot.instrument.samplePath = instrument.samplePath;
    slot.instrument.pluginState.clear();
    slot.instrument.gain = slot.gain;

    slot.fxChain.clear();

    for (const auto& sourceFx : instrument.fxChain)
    {
        project::FxConfig fx;
        fx.kind = sourceFx.kind == "plugin" ? project::FxKind::plugin
                                            : project::FxKind::builtin;
        fx.id = sourceFx.id;
        fx.name = sourceFx.name;
        fx.bypassed = sourceFx.bypassed;
        fx.parameters = sourceFx.parameters;
        slot.fxChain.push_back (std::move (fx));
    }
}

}

void importMappingPatches (const Controller::SetMapping& document,
                           std::map<int, project::SlotConfig>& slots)
{
    for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
    {
        const auto* binding = document.padBinding (pad);

        if (binding == nullptr)
            continue;

        const auto* instrument = document.instrumentFor (*binding);

        if (instrument == nullptr)
            continue;

        const int slotIndex = instrument->slot >= 0 ? instrument->slot : binding->slot;

        if (slotIndex < 0 || slotIndex >= maximumProjectSlots)
            continue;

        auto& slot = slots[slotIndex];
        slot = project::SlotConfig {};
        slot.index = slotIndex;
        slot.active = slotIndex == 0;
        importInstrument (*instrument, slot);
    }
}

}
