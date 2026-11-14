#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>

namespace ihub::plugins
{
    class PluginEditorWindow final : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow (const juce::String& windowTitle, std::function<void ()> userClosed);
        ~PluginEditorWindow () override;

        void closeButtonPressed () override;
        void userTriedToCloseWindow () override;

        void resetCloseRequested () noexcept
        {
            closeAlreadyRequested = false;
        }

        static int getLiveWindowCount () noexcept;

    private:
        std::function<void ()> onUserClosed;
        bool closeAlreadyRequested = false;

        inline static std::atomic<int> liveWindowCount{0};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorWindow)
    };

}
