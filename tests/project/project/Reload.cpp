#include "Reload.h"

#include "Support.h"

#include "app/ProjectSetup.h"
#include "controller/Mapping.h"
#include "project/Serializer.h"

#include <chrono>
#include <string>
#include <thread>

namespace ProjectTest
{

void runReloadChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto projectFile = directory.getChildFile ("Reload.ihub");
    const auto replacementFile = directory.getChildFile ("Reload-replacement.ihub");

    juce::String error;
    bool created = false;

    check (ihub::app::ensureProjectFile (projectFile, error, &created),
           "the reload project imports: " + error.toStdString());

    ihub::project::SetFile replacement;
    check (ihub::project::Serializer::load (projectFile, replacement, error),
           "the reload project loads: " + error.toStdString());

    const auto position = replacement.mappingYaml.find ("instrument: empty-0");
    check (position != std::string::npos,
           "the mapping contains the pad-0 empty binding");

    if (position != std::string::npos)
        replacement.mappingYaml.replace (position, std::string ("instrument: empty-0").size(),
                                         "instrument: empty-1");

    check (ihub::project::Serializer::save (replacement, replacementFile, error),
           "the replacement project saves: " + error.toStdString());

    Controller::Mapping mapping;
    Controller::MappingError mappingError;

    check (mapping.loadProject (projectFile.getFullPathName().toStdString(), &mappingError),
           "the mapping loads from the project container"
               + (mappingError.isEmpty()
                      ? std::string()
                      : " (" + mappingError.message + ")"));

    const auto initial = mapping.getPadBinding (0);
    check (initial.has_value() && initial->instrument == "empty-0" && initial->slot == 0,
           "pad 0 starts on row1/empty-0 / slot 0");

    check (mapping.startWatching (20), "the project watcher starts");

    std::this_thread::sleep_for (std::chrono::milliseconds (40));
    check (projectFile.deleteFile(), "the live project file is removed");
    check (replacementFile.moveFileTo (projectFile), "the replacement takes its place");

    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (1200);
    bool rerouted = false;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto binding = mapping.getPadBinding (0);

        if (binding.has_value() && binding->instrument == "empty-1"
            && binding->slot == 1)
        {
            rerouted = true;
            break;
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }

    check (rerouted, "replacing the project re-routes pad 0 within the watch interval");
    check (mapping.getLastError().isEmpty(), "the hot reload reports no error");

    const auto reroutedBinding = mapping.getPadBinding (0);
    check (reroutedBinding.has_value() && reroutedBinding->slot == 1,
           "the published mapping points pad 0 at slot 1");

    mapping.stopWatching();
    check (! mapping.isWatching(), "the project watcher stops");

    directory.deleteRecursively();
}

}
