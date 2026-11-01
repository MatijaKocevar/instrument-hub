#pragma once

#include "audio/DeviceManager.h"

#include <optional>
#include <string>
#include <vector>

namespace AudioEngine::detail
{

/** A channel mask together with the number of selected channels. */
struct ChannelMap
{
    juce::BigInteger mask;
    int count = 0;
};

/** Validates a requested channel list against the device's channel count. */
bool validateChannels (const std::vector<int>& channels,
                       int available,
                       const char* name,
                       std::string& error);

/** Resolves the channel map a `POST /audio` patch asks for.

    A requested list wins; otherwise a device change (or an empty current mask)
    falls back to the first `defaultChannels` channels, and a plain format
    change keeps the current mapping.
*/
ChannelMap resolveChannelMap (const std::optional<std::vector<int>>& requested,
                              const juce::BigInteger& current,
                              int availableChannels,
                              int defaultChannels,
                              bool deviceChanged);

} // namespace AudioEngine::detail
