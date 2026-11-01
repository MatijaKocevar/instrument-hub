#pragma once

#include "Modes.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace ihub::app
{

class UiModeController
{
public:
    using FinishCallback = std::function<void (int returnValue)>;

    UiModeController (AppMode mode,
                      juce::DocumentWindow& window,
                      juce::Component& content,
                      juce::WebBrowserComponent& webView,
                      juce::File snapshotFile,
                      FinishCallback onFinish);

    void start();

    int getTimerIntervalMs() const noexcept;

    void pageLoaded (const juce::String& url);

    void tick();

private:
    void checkUi();
    void checkSnapshot();
    void checkSelftest();
    void failSnapshot (const juce::String& reason);

    AppMode mode;
    juce::DocumentWindow& window;
    juce::Component& content;
    juce::WebBrowserComponent& webView;
    juce::File snapshotFile;
    FinishCallback finish;

    bool uiCheckPassed = false;
    bool snapshotPageLoaded = false;
    bool snapshotProbePassed = false;
    bool evaluationInFlight = false;

    double uiCheckDeadline = 0.0;
    double snapshotDeadline = 0.0;
    double snapshotCaptureAt = 0.0;

    juce::String lastLoadedUrl;
    juce::String lastHeading;
};

}
