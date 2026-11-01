#pragma once

#include "Modes.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace ihub::app
{

/** The timer-driven state machine behind `--snapshot`, `--ui-check` and
    `--selftest`.

    The application owns the window and its timer; this class owns the
    deadlines, the DOM probe and the pixel checks, and reports the final
    process return value through `FinishCallback`.
*/
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

    /** Records the deadline, or prints the selftest banner. */
    void start();

    /** Timer interval the application should use. */
    int getTimerIntervalMs() const noexcept;

    /** Web view page-load notification. */
    void pageLoaded (const juce::String& url);

    /** One timer tick. */
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

} // namespace ihub::app
