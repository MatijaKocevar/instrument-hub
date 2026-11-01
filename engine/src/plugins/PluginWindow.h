#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <memory>

namespace ihub::plugins
{
class PluginInstrument;
class PluginEditorWindow;
class WindowCallbackToken;

class PluginWindow
{
public:
    PluginWindow();
    ~PluginWindow();

    bool open (juce::AudioProcessor& processor,
               const juce::String& title = {},
               juce::Component* owner = nullptr,
               juce::String* error = nullptr);

    bool open (PluginInstrument& instrument,
               const juce::String& title = {},
               juce::Component* owner = nullptr,
               juce::String* error = nullptr);

    const juce::String& getLastError() const noexcept { return lastError; }

    void close();

    bool isOpen() const noexcept             { return window != nullptr; }

    juce::AudioProcessorEditor* getEditor() const noexcept;

    juce::Component* getWindowComponent() const noexcept;

    juce::AudioProcessor* getProcessor() const noexcept  { return processor; }

    std::function<void()> onClose;

    std::function<void()> onBeforeClose;

    static int getOpenWindowCount() noexcept;

    static int getLiveInstanceCount() noexcept;

private:
    bool showEditor (juce::AudioProcessor& processor,
                     const juce::String& title,
                     juce::Component* owner);

    void destroyWindow();

    void clearInstrumentCallback() noexcept;

    std::unique_ptr<PluginEditorWindow> window;
    juce::AudioProcessor* processor = nullptr;
    PluginInstrument* instrument = nullptr;
    mutable juce::String lastError;
    std::shared_ptr<WindowCallbackToken> lifetime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

}
