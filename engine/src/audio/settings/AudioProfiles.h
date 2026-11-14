#pragma once

#include "settings/SettingsStore.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <vector>

namespace AudioEngine::detail
{

    juce::String soundcardKeyFor (const juce::String& deviceName);

    juce::String audioProfileLabel (const juce::String& deviceType, const juce::String& deviceName);

    const ihub::settings::AudioProfile*
    findAudioProfile (const std::vector<ihub::settings::AudioProfile>& profiles,
                      const juce::String& deviceType,
                      const juce::String& deviceName);

    void upsertAudioProfile (std::vector<ihub::settings::AudioProfile>& profiles,
                             ihub::settings::AudioProfile profile);

    std::vector<int> activeChannelList (const juce::BigInteger& mask);

}
