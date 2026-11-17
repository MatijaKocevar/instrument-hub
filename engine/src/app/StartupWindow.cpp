#include "StartupWindow.h"

namespace ihub::app {

std::optional<juce::Rectangle<int>> leftmostDisplayBounds() {
  const auto& displays = juce::Desktop::getInstance().getDisplays().displays;

  if (displays.size() < 2) return std::nullopt;

  const juce::Displays::Display* leftmost = nullptr;

  for (const auto& display : displays)
    if (leftmost == nullptr ||
        display.userBounds.getX() < leftmost->userBounds.getX())
      leftmost = &display;

  if (leftmost == nullptr) return std::nullopt;

  return leftmost->userBounds.toNearestInt();
}

bool startMaximisedOnLeftDisplay(juce::DocumentWindow& window) {
  const auto bounds = leftmostDisplayBounds();

  if (!bounds.has_value()) return false;

  window.setBounds(*bounds);
  window.setFullScreen(true);
  return true;
}

}  // namespace ihub::app
