#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace AudioEngine::detail
{

struct PcmHint
{
    juce::String name;
    juce::String description;
};

std::vector<PcmHint> readAlsaPcmHints();

juce::String juceDeviceNameForPcm (const std::vector<PcmHint>& hints,
                                   const juce::String& pcmName);

juce::String pcmNameForJuceDescription (const std::vector<PcmHint>& hints,
                                        const juce::String& juceName);

} // namespace AudioEngine::detail
