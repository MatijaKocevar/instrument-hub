#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ihub::app
{

int countDistinctColours (const juce::Image& image);

int countBrightPixels (const juce::Image& image);

bool looksRendered (const juce::Image& image);

juce::Image captureSnapshot (juce::Component& component);

bool writeSnapshot (const juce::Image& image, const juce::File& file);

}
