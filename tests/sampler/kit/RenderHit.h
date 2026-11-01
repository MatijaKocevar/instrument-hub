#pragma once

#include "sampler/KitLoader.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace KitTest
{

juce::AudioBuffer<float> renderHit (const ihub::sampler::ResolvedHit& hit);

}
