#pragma once

#include "PatchState.h"
#include "PluginPageState.h"
#include "ProjectState.h"
#include "TrackState.h"
#include "TransportState.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Api
{

struct SlotRuntimeState
{
    bool armed = false;
    float gain = 0.0f;
    std::string state = "idle";
    std::string warmState = "cold";
    bool warm = false;
    bool editorOpen = false;
    std::string error;
};

using SlotRuntimeProvider = std::function<SlotRuntimeState (int slot)>;

using TransportProvider = std::function<TransportState()>;

struct StateSnapshot
{
    int schemaVersion = 1;
    std::uint64_t revision = 0;
    std::string applicationName = "Instrument Hub";
    std::string applicationVersion = "0.1.0";

    std::string layout = "families";

    int armedSlot = -1;
    std::vector<SlotSnapshot> slots;
    std::vector<TrackState> tracks;
    std::vector<PluginPageStatus> pages;

    std::vector<FamilyState> families;

    SettingsState settings;
    TransportState transport;
    ProjectSaveState project;

    double uptimeMs = 0.0;
};

struct SlotMeter
{
    int slot = 0;
    float peak = 0.0f;
    float gain = 0.0f;
    bool armed = false;
};

struct MeterFrame
{
    std::uint64_t sequence = 0;
    double timestampMs = 0.0;
    float masterPeak = 0.0f;
    std::vector<SlotMeter> slots;
};

using MeterProvider = std::function<MeterFrame()>;

}
