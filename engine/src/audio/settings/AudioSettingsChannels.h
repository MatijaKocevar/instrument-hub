#pragma once

#include "audio/DeviceManager.h"

#include <optional>
#include <string>
#include <vector>

namespace AudioEngine::detail {

struct ChannelMap {
  juce::BigInteger mask;
  int count = 0;
};

bool validateChannels(const std::vector<int>& channels, int available,
                      const char* name, std::string& error);

ChannelMap resolveChannelMap(const std::optional<std::vector<int>>& requested,
                             const juce::BigInteger& current,
                             int availableChannels, int defaultChannels,
                             bool deviceChanged);

}  // namespace AudioEngine::detail
