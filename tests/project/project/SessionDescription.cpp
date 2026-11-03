#include "SessionDescription.h"

#include "Support.h"

#include "SessionConfig.h"
#include "app/ProjectSetup.h"
#include "app/project/ProjectImport.h"
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

void checkTrackSession (const juce::File& projectFile,
                        const Controller::SetMapping& mapping,
                        const std::string& mappingText)
{
    ihub::SessionConfig config;
    juce::String error;

    check (ihub::app::loadProjectSession (projectFile, config, error),
           "the track project builds a session description: "
               + error.toStdString());
    check (config.layout == ihub::SessionLayout::tracks,
           "the track project reports the track layout");
    check (config.slots.size() == 8,
           "the track session has exactly eight slots (got "
               + std::to_string (config.slots.size()) + ")");
    check (config.warmSlots
               == std::vector<int> { 0, 1, 2, 3, 4, 5, 6, 7 },
           "the track session protects all eight warm slots");
    check (mapping.layout == Controller::MappingLayout::tracks,
           "the track mapping selects the track layout");
    check (mapping.countTracks() == 8,
           "the track mapping declares eight tracks");
    check (mapping.countPads() == 64,
           "the track mapping binds all 64 pads");
    check (mappingText.find ("role: volume") != std::string::npos,
           "the track mapping declares the stepped volume pads");

    for (int slotIndex = 0; slotIndex < 8 && slotIndex < static_cast<int> (config.slots.size());
         ++slotIndex)
    {
        const auto& slot = config.slots[static_cast<std::size_t> (slotIndex)];
        const auto& track = mapping.tracks[static_cast<std::size_t> (slotIndex)];
        const auto label = std::to_string (slotIndex);

        check (slot.kind == ihub::SessionSlotKind::empty,
               "track slot " + label + " builds as an empty source");
        check (slot.fxChain.empty(),
               "track slot " + label + " starts without FX");
        check (slot.name == track.name,
               "track slot " + label + " keeps its track name");
        check (slot.family == track.id
                   && slot.familyName == track.name
                   && slot.patch == track.id
                   && slot.patchName == track.name,
               "track slot " + label + " keeps its track identity");
        check (nearlyEqual (slot.gain, 1.0f) && nearlyEqual (slot.pan, 0.0f),
               "track slot " + label + " starts at unity gain and centre pan");
        check (slot.colorIndex == 14,
               "track slot " + label + " starts with the blank track colour");
    }
}

void checkLegacySession (const juce::File& projectFile,
                         const juce::File& mappingFile)
{
    Controller::SetMapping document;
    Controller::MappingError mappingError;
    const auto mappingText = mappingFile.loadFileAsString().toStdString();

    check (Controller::parseSetMappingYaml (mappingText, document, mappingError),
           "the starter mapping parses for comparison");

    ihub::SessionConfig config;
    juce::String error;

    check (ihub::app::loadProjectSession (projectFile, config, error),
           "the starter project builds a session description: "
               + error.toStdString());
    check (config.layout == ihub::SessionLayout::families,
           "the starter project reports the family layout");
    check (config.slots.size() == 64,
           "the starter session keeps 64 slots (got "
               + std::to_string (config.slots.size()) + ")");
    check (config.warmSlots.empty(),
           "the starter session keeps the neighbour warm policy");

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
           "the starter session keeps eight families (got "
               + std::to_string (families.size()) + ")");
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

    const auto mappingText = bundledTracksSetFile().loadFileAsString().toStdString();
    check (Controller::parseSetMappingYaml (mappingText, document, mappingError),
           "the track mapping parses for comparison");

    checkTrackSession (projectFile, document, mappingText);

    ihub::project::SetFile starter;

    check (ihub::app::importBundledSet (bundledMappingFile(),
                                        bundledPluginStateDirectory(),
                                        bundledSourcesFile(),
                                        starter,
                                        error),
           "the starter set imports for a legacy session: "
               + error.toStdString());

    const auto starterProject = directory.getChildFile ("Starter.ihub");
    check (ihub::project::Serializer::save (starter, starterProject, error),
           "the starter project saves: " + error.toStdString());

    checkLegacySession (starterProject, bundledMappingFile());

    directory.deleteRecursively();
}

}
