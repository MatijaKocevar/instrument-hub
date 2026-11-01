#include "PluginEditorWindow.h"

#include <utility>

namespace ihub::plugins
{
namespace
{
/** Background used behind an editor that does not fill the whole window. */
const juce::Colour pluginWindowBackground { 0xff1e1e24 };
} // namespace

PluginEditorWindow::PluginEditorWindow (const juce::String& windowTitle,
                                        std::function<void()> userClosed)
    : juce::DocumentWindow (windowTitle,
                            pluginWindowBackground,
                            juce::DocumentWindow::allButtons),
      onUserClosed (std::move (userClosed))
{
    setUsingNativeTitleBar (true);
    setResizable (false, false);

    liveWindowCount.fetch_add (1, std::memory_order_relaxed);
}

PluginEditorWindow::~PluginEditorWindow()
{
    liveWindowCount.fetch_sub (1, std::memory_order_relaxed);
}

void PluginEditorWindow::closeButtonPressed()
{
    if (closeAlreadyRequested)
        return;

    closeAlreadyRequested = true;

    // The handler destroys this window, which must not happen while its
    // own close-button callback is still on the stack.
    if (onUserClosed)
    {
        auto callback = onUserClosed;
        juce::MessageManager::callAsync ([callback] { callback(); });
    }
}

void PluginEditorWindow::userTriedToCloseWindow()
{
    closeButtonPressed();
}

int PluginEditorWindow::getLiveWindowCount() noexcept
{
    return liveWindowCount.load (std::memory_order_relaxed);
}

} // namespace ihub::plugins
