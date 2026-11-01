#include "audio/settings/AudioSettingsChannels.h"

namespace AudioEngine::detail
{

namespace
{
juce::BigInteger makeMask (const std::vector<int>& channels)
{
    juce::BigInteger mask;

    for (const auto channel : channels)
        if (channel >= 0)
            mask.setBit (channel, true);

    return mask;
}
}

bool validateChannels (const std::vector<int>& channels,
                       int available,
                       const char* name,
                       std::string& error)
{
    if (channels.empty())
    {
        error = std::string (name) + " must select at least one channel";
        return false;
    }

    for (const auto channel : channels)
    {
        if (channel < 0 || channel >= available)
        {
            error = std::string (name) + " contains channel "
                    + juce::String (channel).toStdString()
                    + " but the device only exposes "
                    + juce::String (available).toStdString();
            return false;
        }
    }

    return true;
}

ChannelMap resolveChannelMap (const std::optional<std::vector<int>>& requested,
                              const juce::BigInteger& current,
                              int availableChannels,
                              int defaultChannels,
                              bool deviceChanged)
{
    ChannelMap map;

    if (requested.has_value())
    {
        map.mask = makeMask (*requested);
        map.count = static_cast<int> (requested->size());
    }
    else if (deviceChanged || current.isZero())
    {
        map.count = juce::jlimit (0, availableChannels, defaultChannels);
        map.mask = DeviceSetup::channelMask (map.count);
    }
    else
    {
        map.mask = current;
        map.count = current.countNumberOfSetBits();
    }

    return map;
}

}
