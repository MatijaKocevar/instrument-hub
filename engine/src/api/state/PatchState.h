#pragma once

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace Api
{

    struct SlotSnapshot
    {
        int index = 0;
        std::string name;
        bool armed = false;

        std::string state = "idle";

        std::string warmState = "cold";
        bool warm = false;

        std::string family;
        std::string familyName;
        std::string patch;
        std::string patchName;

        float gain = 0.0f;

        int latencySamples = 0;
        double latencyMs = 0.0;

        bool editorOpen = false;

        std::string error;
    };

    struct PatchInstrumentState
    {
        std::string id;
        std::string name;
        std::string kind = "empty";
        std::string pluginFormat = "VST3";
        std::string pluginId;
        std::string pluginName;
        std::string pluginManufacturer;
        std::string pluginVersion;
        std::string samplePath;
    };

    struct PatchSourceRequest
    {
        std::string kind = "empty";
        std::string name;
        std::string path;
        double frequency = 0.0;
        int inputChannel = 0;
        std::string pluginFormat = "VST3";
        std::string pluginId;
        std::string pluginName;
        std::string pluginManufacturer;
        std::string pluginVersion;
        std::string samplePath;
    };

    struct PatchState
    {
        int slot = -1;
        std::string id;
        std::string name;
        std::string family;
        std::string familyName;

        PatchInstrumentState instrument;

        double volumeDb = 0.0;
        double pan = 0.0;
        bool mute = false;
        bool solo = false;

        int colorIndex = 0;

        std::string warmState = "cold";
        bool warm = false;

        bool editorOpen = false;

        std::string error;

        int latencySamples = 0;
        double latencyMs = 0.0;
    };

    struct FamilyState
    {
        std::string id;
        std::string name;
        std::vector<PatchState> patches;
    };

    struct PatchStateCapture
    {
        int slot = -1;
        bool captured = false;

        juce::int64 sizeBytes = 0;
    };

}
