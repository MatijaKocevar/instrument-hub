#include "UiModes.h"

#include "Constants.h"
#include "SnapshotCapture.h"

#include "Bridge/WebView.h"

#include <iostream>
#include <utility>

namespace ihub::app
{

UiModeController::UiModeController (AppMode modeToUse,
                                    juce::DocumentWindow& windowToUse,
                                    juce::Component& contentToUse,
                                    juce::WebBrowserComponent& webViewToUse,
                                    juce::File snapshotFileToUse,
                                    FinishCallback onFinish)
    : mode (modeToUse),
      window (windowToUse),
      content (contentToUse),
      webView (webViewToUse),
      snapshotFile (std::move (snapshotFileToUse)),
      finish (std::move (onFinish))
{
}

void UiModeController::start()
{
    if (mode == AppMode::snapshot)
    {
        snapshotDeadline = juce::Time::getMillisecondCounterHiRes() + kSnapshotTimeoutMs;
    }
    else if (mode == AppMode::uiCheck)
    {
        uiCheckDeadline = juce::Time::getMillisecondCounterHiRes() + kUiCheckTimeoutMs;
    }
    else if (mode == AppMode::selftest)
    {
        std::cout << "selftest: window opened " << kWindowWidth << "x" << kWindowHeight
                  << ", quitting in 3 s" << std::endl;
    }
}

int UiModeController::getTimerIntervalMs() const noexcept
{
    switch (mode)
    {
        case AppMode::snapshot: return 100;
        case AppMode::uiCheck:  return 200;
        case AppMode::selftest: return 3000;
        case AppMode::gui:
        case AppMode::serve:
        case AppMode::pluginScan:
        case AppMode::rtStrict:
        case AppMode::listDevices:
        case AppMode::measureLatency:
        case AppMode::diagnostics:
        case AppMode::sessionSmoke:
            return 0;
    }

    return 0;
}

void UiModeController::pageLoaded (const juce::String& url)
{
    lastLoadedUrl = url;
    snapshotPageLoaded = true;
}

void UiModeController::tick()
{
    if (mode == AppMode::snapshot)
    {
        checkSnapshot();
        return;
    }

    if (mode == AppMode::uiCheck)
    {
        checkUi();
        return;
    }

    if (mode == AppMode::selftest)
        checkSelftest();
}

void UiModeController::checkSelftest()
{
    if (! window.isOnDesktop() || ! window.isVisible())
    {
        std::cerr << "selftest: FAILED, window is not on the desktop" << std::endl;
        finish (1);
        return;
    }

    const auto bounds = window.getScreenBounds();
    std::cout << "selftest: window on desktop at " << bounds.getWidth() << "x"
              << bounds.getHeight() << ", quitting cleanly" << std::endl;
    finish (0);
}

void UiModeController::checkUi()
{
    if (juce::Time::getMillisecondCounterHiRes() > uiCheckDeadline)
    {
        std::cerr << "ui-check: FAILED, heading not found after "
                  << static_cast<int> (kUiCheckTimeoutMs) << " ms"
                  << " (last heading: \"" << lastHeading << "\""
                  << ", url: " << lastLoadedUrl << ")" << std::endl;
        finish (1);
        return;
    }

    if (evaluationInFlight)
        return;

    evaluationInFlight = true;

    webView.evaluateJavascript (
        "(() => {"
        "  const el = document.querySelector('[data-testid=\"sidebar-title\"]');"
        "  return el ? el.textContent.trim() : '';"
        "})()",
        [this] (const juce::WebBrowserComponent::EvaluationResult& result)
        {
            evaluationInFlight = false;

            if (uiCheckPassed)
                return;

            if (const auto* value = result.getResult())
            {
                const auto heading = value->toString().trim();

                if (heading.isNotEmpty())
                    lastHeading = heading;

                if (heading == "Instrument Hub")
                {
                    uiCheckPassed = true;

                    std::cout << "ui-check: PASS heading=\"" << heading << "\""
                              << " url=" << lastLoadedUrl
                              << " mode="
                              << (WebView::isDevelopmentBuild() ? "debug-dev-server"
                                                                : "release-embedded")
                              << std::endl;

                    finish (0);
                }
            }
        });
}

void UiModeController::failSnapshot (const juce::String& reason)
{
    std::cerr << "snapshot: FAILED, " << reason
              << " (url: " << lastLoadedUrl << ")" << std::endl;
    finish (1);
}

void UiModeController::checkSnapshot()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (now > snapshotDeadline)
    {
        failSnapshot ("the UI did not render within "
                      + juce::String (static_cast<int> (kSnapshotTimeoutMs)) + " ms");
        return;
    }

    if (! snapshotPageLoaded || evaluationInFlight)
        return;

    if (! snapshotProbePassed)
    {
        evaluationInFlight = true;

        webView.evaluateJavascript (
            "(() => {"
            "  const el = document.querySelector('[data-testid=\"grid-cell\"]');"
            "  return el ? 'Instrument grid' : '';"
            "})()",
            [this] (const juce::WebBrowserComponent::EvaluationResult& result)
            {
                evaluationInFlight = false;

                if (snapshotProbePassed)
                    return;

                const auto* value = result.getResult();

                if (value == nullptr)
                    return;

                const auto heading = value->toString().trim();

                if (heading.isNotEmpty())
                    lastHeading = heading;

                if (heading == "Instrument grid")
                {
                    snapshotProbePassed = true;
                    snapshotCaptureAt = juce::Time::getMillisecondCounterHiRes()
                                        + kSnapshotSettleMs;

                    window.toFront (true);
                }
            });

        return;
    }

    if (now < snapshotCaptureAt)
        return;

    const auto image = captureSnapshot (content);
    const auto colours = countDistinctColours (image);
    const auto bright = countBrightPixels (image);

    if (image.isNull()
        || image.getWidth() != kSnapshotWidth
        || image.getHeight() != kSnapshotHeight
        || ! looksRendered (image))
    {
        if (now <= snapshotDeadline)
            return;

        if (image.isNull())
            failSnapshot ("capture produced no image");
        else if (image.getWidth() != kSnapshotWidth
                 || image.getHeight() != kSnapshotHeight)
            failSnapshot ("capture is " + juce::String (image.getWidth()) + "x"
                          + juce::String (image.getHeight()) + ", expected "
                          + juce::String (kSnapshotWidth) + "x"
                          + juce::String (kSnapshotHeight));
        else
            failSnapshot ("capture is blank (" + juce::String (colours)
                          + " distinct colours, " + juce::String (bright)
                          + " bright samples)");
        return;
    }

    if (! writeSnapshot (image, snapshotFile))
    {
        failSnapshot ("could not write " + snapshotFile.getFullPathName());
        return;
    }

    std::cout << "snapshot: wrote " << snapshotFile.getFullPathName()
              << " " << image.getWidth() << "x" << image.getHeight()
              << " colours=" << colours
              << " bright=" << bright
              << " url=" << lastLoadedUrl
              << " mode="
              << (WebView::isDevelopmentBuild() ? "debug-dev-server"
                                                : "release-embedded")
              << std::endl;

    finish (0);
}

}
