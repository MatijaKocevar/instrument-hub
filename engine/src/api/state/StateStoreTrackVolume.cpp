#include "api/StateStore.h"

#include <array>
#include <cmath>
#include <utility>

namespace Api
{

namespace
{
constexpr double overGain = 1.9952623149688795;

const std::array<double, trackVolumeMaximumStep + 1> stepGains {
    0.0, 0.25, 0.5, 0.75, 1.0, overGain
};
}

double trackGainForStep (int step) noexcept
{
    if (step < trackVolumeMinimumStep || step > trackVolumeMaximumStep)
        return 1.0;

    return stepGains[static_cast<std::size_t> (step)];
}

double trackDbForStep (int step) noexcept
{
    const double gain = trackGainForStep (step);

    if (gain <= 0.0)
        return -60.0;

    return 20.0 * std::log10 (gain);
}

int trackStepForGain (double gain) noexcept
{
    if (gain <= 0.001)
        return 0;

    if (gain >= 1.4)
        return trackVolumeMaximumStep;

    int closest = 4;
    double bestDistance = std::abs (gain - 1.0);

    for (int step = 1; step < 4; ++step)
    {
        const double distance = std::abs (gain - stepGains[static_cast<std::size_t> (step)]);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            closest = step;
        }
    }

    return closest;
}

void StateStore::setTrackGainHandler (TrackGainHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    trackGainHandler = std::move (handler);
}

bool StateStore::setTrackVolumeStep (int index, int step, std::string& error)
{
    int slot = -1;
    TrackGainHandler handler;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (index < 0 || index >= static_cast<int> (tracks.size()))
        {
            error = "track index is out of range";
            return false;
        }

        if (step < trackVolumeMinimumStep || step > trackVolumeMaximumStep)
        {
            error = "step must be between 0 and 5";
            return false;
        }

        auto& track = tracks[static_cast<std::size_t> (index)];

        if (track.slot < 0 || track.slot >= numSlots)
        {
            error = "track slot is out of range";
            return false;
        }

        track.volumeStep = step;
        slot = track.slot;
        slotMixes[static_cast<std::size_t> (slot)].volumeDb = trackDbForStep (step);
        handler = trackGainHandler;
    }

    if (handler != nullptr)
        handler (slot, static_cast<float> (trackGainForStep (step)));

    notifyEdited ("track", slot);
    notifyChanged ("track", index);
    return true;
}

}
