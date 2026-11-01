#pragma once

#include "PatchState.h"
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
};

/** Supplies one slot's live state. */
using SlotRuntimeProvider = std::function<SlotRuntimeState (int slot)>;

/** Supplies the live transport. */
using TransportProvider = std::function<TransportState()>;

/** A consistent-enough snapshot of everything `GET /state` returns. */
struct StateSnapshot
{
    int schemaVersion = 1;
    std::string applicationName = "Instrument Hub";
    std::string applicationVersion = "0.1.0";

    int armedSlot = -1;
    std::vector<SlotSnapshot> slots;

    /** 8 families x 8 patches for the Launchpad's 64-pad grid. */
    std::vector<FamilyState> families;

    SettingsState settings;
    TransportState transport;

    double uptimeMs = 0.0;
};

/** One slot in a meter frame. */
struct SlotMeter
{
    int slot = 0;
    float peak = 0.0f;
    float gain = 0.0f;
    bool armed = false;
};

/** One coalesced meter frame, broadcast at `meterHz` (30 by default). */
struct MeterFrame
{
    std::uint64_t sequence = 0;
    double timestampMs = 0.0;
    float masterPeak = 0.0f;
    std::vector<SlotMeter> slots;
};

/** Supplies meter values to the broadcaster.

    Called on the broadcaster thread, never from the audio thread. A provider
    that reads an engine must only read lock-free/atomic values: the API server
    must not add locks to the audio path.
*/
using MeterProvider = std::function<MeterFrame()>;

} // namespace Api
