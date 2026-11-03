#include "project/ProjectScenario.h"

#include "Check.h"

#include "Session.h"
#include "SessionConfig.h"
#include "app/project/ProjectImport.h"
#include "app/project/ProjectSession.h"
#include "project/Serializer.h"

#include <juce_core/juce_core.h>

#include <string>

#ifndef IHUB_SHARED_SET_YAML
 #define IHUB_SHARED_SET_YAML "shared/set.yaml"
#endif

namespace PadAudio
{

void runProjectScenario()
{
    const auto directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("e2e-project-" + juce::Uuid().toString());
    directory.createDirectory();

    const auto projectFile = directory.getChildFile ("Live.ihub");
    const auto mappingFile = juce::File (IHUB_SHARED_SET_YAML);

    juce::String error;
    ihub::project::SetFile starter;

    E2E::check (ihub::app::importBundledSet (
                    mappingFile,
                    mappingFile.getSiblingFile ("set.plugin-state"),
                    mappingFile.getSiblingFile ("set.sources.json"),
                    starter,
                    error),
                "the starter set imports for the legacy project: "
                    + error.toStdString());
    E2E::check (ihub::project::Serializer::save (starter, projectFile, error),
                "the legacy starter project saves: " + error.toStdString());

    ihub::SessionConfig config;

    E2E::check (ihub::app::loadProjectSession (projectFile, config, error),
                "the project builds the live session description: " + error.toStdString());
    E2E::check (config.slots.size() == 64,
                "the project session keeps 64 slots");

    config.initialSlot = 10;

    ihub::Session session;

    E2E::check (session.build (config, error),
                "the project session builds without an audio device: " + error.toStdString());
    E2E::check (session.getNumSlots() == 64,
                "the built session has 64 slots");

    const auto& slots = session.getSlotConfigs();

    E2E::check (! slots.empty() && slots[0].family == "keys"
                    && slots[0].patch == "piano" && slots[0].colorIndex == 0,
                "slot 0 stays keys/piano on colour 0");

    int pluginStates = 0;

    for (const auto& slot : slots)
        if (slot.kind == ihub::SessionSlotKind::plugin
            && slot.pluginState.getSize() > 0)
            ++pluginStates;

    E2E::check (pluginStates == 3,
                "all three imported plug-in sounds reach the built session");

    if (slots.size() > 56)
        E2E::check (session.getPluginStateSize (56) > 0,
                    "slot 56 exposes its stored plug-in state");

    directory.deleteRecursively();
}

}
