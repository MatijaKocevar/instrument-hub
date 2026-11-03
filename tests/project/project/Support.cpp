#include "Support.h"

#include <iostream>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

#ifndef IHUB_PLUGIN_STATE_DIR
 #define IHUB_PLUGIN_STATE_DIR "shared/set.plugin-state"
#endif

#ifndef IHUB_SOURCES_JSON
 #define IHUB_SOURCES_JSON "shared/set.sources.json"
#endif

namespace ProjectTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

juce::File makeTemporaryDirectory()
{
    const auto directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("project-test-"
                                              + juce::Uuid().toString());

    directory.createDirectory();
    return directory;
}

juce::File bundledMappingFile()
{
    return juce::File (IHUB_SET_YAML);
}

juce::File bundledPluginStateDirectory()
{
    return juce::File (IHUB_PLUGIN_STATE_DIR);
}

juce::File bundledSourcesFile()
{
    return juce::File (IHUB_SOURCES_JSON);
}

}
