#pragma once

#include "audio/Slot.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub::session
{

    std::unique_ptr<AudioEngine::SlotSource> makeInitToneSource (double frequency, float amplitude);

    std::unique_ptr<AudioEngine::SlotSource> makeSilentSource ();

    std::unique_ptr<AudioEngine::SlotSource>
    makeSamplerSource (const juce::File& file, double sampleRate, juce::String& error);

}
