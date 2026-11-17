#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace AudioEngine::detail {

struct AlsaCard {
    juce::String id;
    juce::String name;
    juce::String pcmName;
    int device = 0;
    bool hasInput = false;
};

std::vector<AlsaCard> enumerateAlsaCards();

juce::String friendlyCardName(const AlsaCard& card);

juce::String cardDescription(const AlsaCard& card);

std::vector<juce::String> cardPcmCandidates(const AlsaCard& card);

int alsaPlaybackChannelCount(const juce::String& pcmName);

int alsaPcmOpenError(const juce::String& pcmName);

}  // namespace AudioEngine::detail
