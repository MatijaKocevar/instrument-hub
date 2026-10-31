#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

/**
    The embedded React UI.

    In Debug builds the component points at the Vite dev server so that a
    browser reload/HMR update follows every save. In Release builds it serves
    the files embedded from `ui/dist` through a JUCE resource provider, so the
    shipped application has no external runtime dependency.

    The class is deliberately thin: all UI logic lives in `ui/`, and the engine
    only owns the host component.
*/
class WebView final : public juce::WebBrowserComponent
{
public:
    using PageLoadedCallback = std::function<void (const juce::String& url)>;

    explicit WebView (PageLoadedCallback onPageLoaded = {});

    /** The URL the build is configured to open. */
    static juce::String initialUrl();

    /** True when the build uses the Vite dev server. */
    static bool isDevelopmentBuild();

    /** Fires after every successful page load. */
    void pageFinishedLoading (const juce::String& url) override;

    /** Logs a failed load instead of silently showing a blank view. */
    bool pageLoadHadNetworkError (const juce::String& errorInfo) override;

private:
    PageLoadedCallback pageLoadedCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebView)
};
