#include "SessionDescription.h"

#include "Support.h"

#include "SessionConfig.h"
#include "app/ProjectSetup.h"
#include "controller/Mapping.h"
#include "controller/MappingYaml.h"
#include "project/Serializer.h"
#include "session/PluginStateFile.h"

#include <cmath>
#include <set>
#include <string>

namespace ProjectTest
{

namespace
{

bool nearlyEqual (float left, float right, float tolerance = 1.0e-4f)
{
    return std::abs (left - right) <= tolerance;
}

float linearGain (float decibels)
{
    return decibels <= -60.0f ? 0.0f : std::pow (10.0f, decibels / 20.0f);
}

}

void runSessionDescriptionChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto projectFile = directory.getChildFile ("Session.ihub");

    juce::String error;
    bool created = false;

    check (ihub::app::ensureProjectFile (projectFile, error, &created),
           "the session project imports: " + error.toStdString());

    Controller::SetMapping document;
    Controller::MappingError mappingError;

    const auto mappingText = bundledBlankSetFile().loadFileAsString().toStdString();
    check (Controller::parseSetMappingYaml (mappingText, document, mappingError),
           "the blank mapping parses for comparison");

    ihub::SessionConfig config;
    check (ihub::app::loadProjectSession (projectFile, config, error),
           "the project builds a session description: " + error.toStdString());
    check (config.slots.size() == 64,
           "the session description has 64 slots (got "
               + std::to_string (config.slots.size()) + ")");

    int mismatches = 0;
    std::set<std::string> families;

    for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
    {
        const auto* binding = document.padBinding (pad);

        if (binding == nullptr)
            continue;

        const auto* instrument = document.instrumentFor (*binding);

        if (instrument == nullptr)
            continue;

        const int slotIndex = instrument->slot >= 0 ? instrument->slot : binding->slot;

        if (slotIndex < 0 || slotIndex >= static_cast<int> (config.slots.size()))
            continue;

        const auto& slot = config.slots[static_cast<std::size_t> (slotIndex)];

        const bool matches = slot.family == binding->family
                             && slot.patch == instrument->id
                             && slot.patchName == instrument->name
                             && slot.name == instrument->name
                             && slot.colorIndex == instrument->resolvedColorIndex
                             && nearlyEqual (slot.gain, linearGain (instrument->volumeDb))
                             && nearlyEqual (slot.pan, instrument->pan)
                             && slot.muted == instrument->muted
                             && slot.solo == instrument->solo;

        if (! matches)
            ++mismatches;

        families.insert (slot.family);
    }

    check (mismatches == 0,
           "every one of the 64 pads keeps its family, name, colour and mixer");
    check (families.size() == 8,
           "the session keeps eight families (got "
               + std::to_string (families.size()) + ")");

    ihub::project::SetFile set;
    check (ihub::project::Serializer::load (projectFile, set, error),
           "the project reloads for plugin-state comparison: " + error.toStdString());

    int pluginSlots = 0;

    for (const auto& slot : set.slots)
    {
        if (slot.instrument.kind != ihub::project::SourceKind::plugin)
            continue;

        ++pluginSlots;

        const auto& live = config.slots[static_cast<std::size_t> (slot.index)];
        check (live.kind == ihub::SessionSlotKind::plugin,
               "slot " + std::to_string (slot.index) + " builds as a plugin");
        check (live.path == juce::String (slot.instrument.pluginPath),
               "slot " + std::to_string (slot.index) + " keeps its plugin bundle");
        check (live.pluginState.getSize() > 0,
               "slot " + std::to_string (slot.index) + " carries its plugin state");
    }

    check (pluginSlots == static_cast<int> (set.pluginStates.size()),
           "every imported plugin state reaches the session description");

    directory.deleteRecursively();
}

}
