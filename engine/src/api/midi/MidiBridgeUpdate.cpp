#include "api/MidiBridge.h"

#include "api/midi/MidiBridgeSupport.h"

#include <algorithm>

namespace Api
{

namespace
{
bool isGridRole (const std::string& role)
{
    return role == "grid";
}

bool isKeyboardRole (const std::string& role)
{
    return role == "keyboard";
}
}

bool MidiBridge::updateInput (const MidiInputPatch& patch, std::string& error)
{
    error.clear();

    const auto inputs = listDevices();
    const auto device = std::find_if (inputs.begin(), inputs.end(),
                                      [&patch] (const MidiInputDevice& input)
                                      {
                                          return input.id == patch.id;
                                      });

    if (device == inputs.end())
    {
        error = "unknown MIDI input: " + patch.id;
        return false;
    }

    const bool launchpadInput = roleForName (juce::String (device->name)) == "launchpad";

    if (launchpadInput
        && (patch.hasRole || patch.hasNoteBase || (patch.hasLearn && patch.learn)))
    {
        error = "the Launchpad has its own grid protocol; no generic pad mapping is needed";
        return false;
    }

    if (patch.hasRole && ! isGridRole (patch.role) && ! isKeyboardRole (patch.role))
    {
        error = "role must be \"keyboard\" or \"grid\"";
        return false;
    }

    if (patch.hasEnabled && ! setInputEnabled (patch.id, patch.enabled, error))
        return false;

    bool clearStoredGrid = false;
    juce::String clearedClient;
    juce::String clearedPort;

    if (patch.hasRole)
    {
        const std::lock_guard<std::mutex> lock (gridMutex);

        if (isGridRole (patch.role))
        {
            auto& entry = grids[patch.id];
            entry.client = juce::String (device->client);
            entry.port = juce::String (device->name);
            entry.dirty = true;
        }
        else
        {
            grids.erase (patch.id);
            clearStoredGrid = true;
            clearedClient = juce::String (device->client);
            clearedPort = juce::String (device->name);
        }
    }

    if (clearStoredGrid)
        removeGridSetting (clearedClient, clearedPort);

    if (patch.hasNoteBase)
    {
        const std::lock_guard<std::mutex> lock (gridMutex);
        const auto entry = grids.find (patch.id);

        if (entry == grids.end())
        {
            error = "assign the grid role before setting a note base";
            return false;
        }

        if (patch.noteBase < -1 || patch.noteBase > MidiGridMap::highestNoteBase)
        {
            error = "noteBase must be between 0 and "
                    + std::to_string (MidiGridMap::highestNoteBase);
            return false;
        }

        entry->second.map.setNoteBase (patch.noteBase);
        entry->second.error.clear();
        entry->second.dirty = true;
    }

    if (patch.hasLearn)
    {
        if (patch.learn && ! patch.hasEnabled && ! setInputEnabled (patch.id, true, error))
            return false;

        const std::lock_guard<std::mutex> lock (gridMutex);
        const auto entry = grids.find (patch.id);

        if (entry == grids.end())
        {
            error = "assign the grid role before learning pads";
            return false;
        }

        if (patch.learn)
        {
            entry->second.map.startLearn();
            entry->second.error.clear();
            entry->second.learnDeadline = std::chrono::steady_clock::now()
                                          + std::chrono::milliseconds (learnTimeoutMs);
        }
        else
        {
            entry->second.map.cancelLearn();
            entry->second.dirty = true;
        }
    }

    persistGridSettings();
    return true;
}

}
