#include "PluginEditorWindow.h"

#include <utility>

namespace ihub::plugins {
namespace {
const juce::Colour pluginWindowBackground{0xff1e1e24};
}

PluginEditorWindow::PluginEditorWindow(const juce::String& windowTitle,
                                       std::function<void()> userClosed)
    : juce::DocumentWindow(windowTitle, pluginWindowBackground,
                           juce::DocumentWindow::allButtons),
      onUserClosed(std::move(userClosed)) {
  setUsingNativeTitleBar(true);
  setResizable(false, false);

  liveWindowCount.fetch_add(1, std::memory_order_relaxed);
}

PluginEditorWindow::~PluginEditorWindow() {
  liveWindowCount.fetch_sub(1, std::memory_order_relaxed);
}

void PluginEditorWindow::closeButtonPressed() {
  if (closeAlreadyRequested) return;

  closeAlreadyRequested = true;

  if (onUserClosed) {
    auto callback = onUserClosed;
    juce::MessageManager::callAsync([callback] { callback(); });
  }
}

void PluginEditorWindow::userTriedToCloseWindow() { closeButtonPressed(); }

int PluginEditorWindow::getLiveWindowCount() noexcept {
  return liveWindowCount.load(std::memory_order_relaxed);
}

}  // namespace ihub::plugins
