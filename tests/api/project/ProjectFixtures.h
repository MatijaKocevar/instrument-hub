#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

#include <string>

namespace ApiTest::ProjectFixture
{

std::string mappingYaml (const std::string& name,
                         const std::string& familyId,
                         const std::string& familyName,
                         const std::string& instrumentId,
                         const std::string& instrumentName,
                         int slot);

std::string mappingYamlFromTemplate (const juce::File& templateFile,
                                     const std::string& familyId,
                                     const std::string& familyName);

ihub::project::SetFile makeSet (const std::string& name,
                                const std::string& yaml,
                                int slot,
                                const std::string& toneName,
                                float gain,
                                float pan,
                                int colorIndex);

bool writeSet (const juce::File& file,
               const ihub::project::SetFile& set,
               juce::String& error);

juce::MemoryBlock readBytes (const juce::File& file);

}
