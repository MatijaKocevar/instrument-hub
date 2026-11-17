#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace ihub::app {

std::optional<juce::Rectangle<int>> leftmostDisplayBounds();

bool startMaximisedOnLeftDisplay(juce::DocumentWindow& window);

}  // namespace ihub::app
