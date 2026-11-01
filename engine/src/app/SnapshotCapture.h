#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ihub::app
{

/** Number of distinct colours in a coarse grid of an image.

    A snapshot of the real UI has many (text antialiasing alone produces
    hundreds); a blank or fallback-coloured component has one or two.
*/
int countDistinctColours (const juce::Image& image);

/** Number of sampled pixels that are clearly brighter than the dark chrome.

    The app background is a dark gradient (#101014 with two faint radial
    washes). The title, labels and panels paint near-white pixels, so a
    background-only frame has none while a real render has thousands.
*/
int countBrightPixels (const juce::Image& image);

/** True when an image contains rendered UI, not just a flat/gradient fill. */
bool looksRendered (const juce::Image& image);

/** Captures the live web view, preferring the native window on Linux. */
juce::Image captureSnapshot (juce::Component& component);

/** Writes `image` as a PNG, creating parent directories. */
bool writeSnapshot (const juce::Image& image, const juce::File& file);

} // namespace ihub::app
