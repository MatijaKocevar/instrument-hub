#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

class WebView final : public juce::WebBrowserComponent
{
public:
    using PageLoadedCallback = std::function<void (const juce::String& url)>;

    explicit WebView (PageLoadedCallback onPageLoaded = {});

    static juce::String initialUrl();

    static bool isDevelopmentBuild();

    void pageFinishedLoading (const juce::String& url) override;

    bool pageLoadHadNetworkError (const juce::String& errorInfo) override;

private:
    PageLoadedCallback pageLoadedCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebView)
};
