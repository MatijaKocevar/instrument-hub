#include "project/ProjectScenario.h"

#include "Check.h"

#include "Session.h"
#include "SessionConfig.h"
#include "app/ProjectSetup.h"

#include <juce_core/juce_core.h>

#include <cstdlib>
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

    setenv ("INSTRUMENT_HUB_SET", IHUB_SHARED_SET_YAML, 1);

    juce::String error;
    bool created = false;

    E2E::check (ihub::app::ensureProjectFile (projectFile, error, &created),
                "the project starts from the bundled set: " + error.toStdString());

    if (! created)
    {
        E2E::check (false, "the project scenario imports a fresh container");
        directory.deleteRecursively();
        return;
    }

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
                    && slots[0].patch == "piano" && slots[0].colorIndex == 1,
                "slot 0 stays keys/piano on colour 1");

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
