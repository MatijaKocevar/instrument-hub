#include "ProjectSession.h"

#include "ProjectLimits.h"
#include "SessionConfig.h"
#include "SessionSlotMapping.h"

#include "controller/MappingYaml.h"
#include "project/Serializer.h"

#include <cmath>
#include <utility>

namespace ihub::app
{

namespace
{

int clampColourIndex (int index) noexcept
{
    return juce::jlimit (0, 15, index);
}

void importMappingSlot (const Controller::InstrumentSpec& instrument,
                        const Controller::PadBinding& binding,
                        const Controller::SetMapping& document,
                        SessionSlotConfig& slot)
{
    slot.family = binding.family;
    slot.patch = instrument.id;
    slot.patchName = instrument.name;
    slot.name = instrument.name.empty() ? instrument.id : instrument.name;
    slot.gain = instrument.volumeDb <= -60.0f
                    ? 0.0f
                    : std::pow (10.0f, instrument.volumeDb / 20.0f);
    slot.pan = instrument.pan;
    slot.muted = instrument.muted;
    slot.solo = instrument.solo;
    slot.colorIndex = clampColourIndex (instrument.resolvedColorIndex);
    slot.instrumentKind = instrument.kind;
    slot.pluginFormat = instrument.pluginFormat.empty() ? "VST3"
                                                         : instrument.pluginFormat;
    slot.pluginId = instrument.pluginId;
    slot.pluginName = instrument.pluginName;
    slot.pluginManufacturer = instrument.pluginManufacturer;
    slot.pluginVersion = instrument.pluginVersion;
    slot.samplePath = instrument.samplePath;

    if (const auto family = document.families.find (binding.family);
        family != document.families.end())
        slot.familyName = family->second.name;

    slot.fxChain.clear();

    for (const auto& sourceFx : instrument.fxChain)
    {
        AudioEngine::FxInsertConfig fx;
        fx.bypassed = sourceFx.bypassed;
        fx.id = sourceFx.id;
        fx.name = sourceFx.name;

        if (sourceFx.kind == "plugin")
        {
            fx.type = AudioEngine::FxInsertType::vst3;
            fx.path = sourceFx.id;
        }
        else if (! AudioEngine::fxInsertTypeFromString (sourceFx.id, fx.type))
        {
            continue;
        }

        for (const auto& [key, value] : sourceFx.parameters)
        {
            if (key == "wet")
                fx.wet = juce::jlimit (0.0f, 1.0f, value);
            else
                fx.parameters[key] = value;
        }

        slot.fxChain.push_back (std::move (fx));
    }
}

}

bool loadProjectSession (const juce::File& projectFile,
                         SessionConfig& config,
                         juce::String& error)
{
    project::SetFile set;

    if (! project::Serializer::load (projectFile, set, error))
        return false;

    Controller::SetMapping document;
    Controller::MappingError mappingError;

    if (! Controller::parseSetMappingYaml (set.mappingYaml, document, mappingError))
    {
        error = "project mapping: line " + juce::String (mappingError.line) + ": "
                + juce::String (mappingError.message);
        return false;
    }

    config.slots.assign (maximumProjectSlots, SessionSlotConfig {});

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

        auto& slot = config.slots[static_cast<std::size_t> (slotIndex)];
        slot = SessionSlotConfig {};
        importMappingSlot (*instrument, *binding, document, slot);
    }

    for (const auto& source : set.slots)
    {
        if (source.index < 0 || source.index >= maximumProjectSlots)
            continue;

        auto& slot = config.slots[static_cast<std::size_t> (source.index)];
        applyProjectSlot (source, slot);

        if (slot.kind != SessionSlotKind::plugin
            || source.instrument.pluginState.empty())
            continue;

        const auto* chunk = set.findPluginState (source.instrument.pluginState);

        if (chunk == nullptr)
        {
            error = "slot " + juce::String (source.index)
                    + " references missing plugin state "
                    + juce::String (source.instrument.pluginState);
            return false;
        }

        slot.pluginState = chunk->data;

        if (slot.pluginFormat.empty())
            slot.pluginFormat = chunk->format;

        if (slot.pluginName.empty())
            slot.pluginName = chunk->name;
    }

    error.clear();
    return true;
}

}
