#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::app
{

    bool importBundledSet (const juce::File& mappingFile,
                           const juce::File& pluginStateDirectory,
                           const juce::File& sourcesFile,
                           ihub::project::SetFile& destination,
                           juce::String& error);

    bool
    ensureProjectFile (const juce::File& projectFile, juce::String& error, bool* created = nullptr);

    bool writeBundledProject (const juce::File& projectFile,
                              const juce::String& name,
                              juce::String& error);

}
