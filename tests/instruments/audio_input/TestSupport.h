#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <string>

namespace AudioInputTest
{

extern int failures;

void check (bool condition, const std::string& what);
double dbfs (double linear);
void fillSine (juce::AudioBuffer<float>& buffer, int channel);

} // namespace AudioInputTest
