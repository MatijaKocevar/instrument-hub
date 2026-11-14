#pragma once

#include "SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project
{

    class Serializer final
    {
    public:
        Serializer () = delete;

        static constexpr const char* manifestEntry = "set.json";
        static constexpr const char* mappingEntry = "mapping.yaml";

        static constexpr juce::int64 maximumManifestBytes = 8 * 1024 * 1024;
        static constexpr juce::int64 maximumMappingBytes = 4 * 1024 * 1024;
        static constexpr juce::int64 maximumPluginStateBytes = 64 * 1024 * 1024;
        static constexpr juce::int64 maximumTotalUncompressedBytes = 256 * 1024 * 1024;

        static bool save (const SetFile& set, const juce::File& file, juce::String& error);

        static bool load (const juce::File& file, SetFile& destination, juce::String& error);
    };

}
