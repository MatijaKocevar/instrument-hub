#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>

namespace ihub::plugins
{
/** The actual desktop window. It owns the editor as its content component, so
    both are destroyed by one `delete` from `PluginWindow::close()`. */
class PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow (const juce::String& windowTitle, std::function<void()> userClosed);
    ~PluginEditorWindow() override;

    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;

    static int getLiveWindowCount() noexcept;

private:
    std::function<void()> onUserClosed;
    bool closeAlreadyRequested = false;

    inline static std::atomic<int> liveWindowCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorWindow)
};

} // namespace ihub::plugins
