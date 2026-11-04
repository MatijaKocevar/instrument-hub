#include "api/StateStore.h"

#include <utility>

namespace Api
{

namespace
{
constexpr std::size_t maximumTrackHotFx = 2;
}

void StateStore::setTracks (std::vector<TrackConfig> next)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);

        for (auto& track : next)
        {
            track.volumeStep = juce::jlimit (trackVolumeMinimumStep,
                                             trackVolumeMaximumStep,
                                             track.volumeStep);

            if (track.hotFx.size() > maximumTrackHotFx)
                track.hotFx.resize (maximumTrackHotFx);
        }

        tracks = std::move (next);
    }

    notifyChanged ("track", -1);
}

std::vector<TrackConfig> StateStore::getTracks() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return tracks;
}

bool StateStore::selectTrack (int index, std::string& error)
{
    int slot = -1;
    ArmHandler handler;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (index < 0 || index >= static_cast<int> (tracks.size()))
        {
            error = "track index is out of range";
            return false;
        }

        slot = tracks[static_cast<std::size_t> (index)].slot;

        if (slot < 0 || slot >= numSlots)
        {
            error = "track slot is out of range";
            return false;
        }

        armedSlot = slot;
        handler = armHandler;
    }

    if (handler != nullptr)
        handler (slot);

    notifyChanged ("track", index);
    return true;
}

}
