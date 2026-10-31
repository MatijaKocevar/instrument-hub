#include "Bridge/WebView.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <iostream>
#include <memory>

namespace
{
constexpr int kWindowWidth = 1200;
constexpr int kWindowHeight = 800;
constexpr double kUiCheckTimeoutMs = 20000.0;

class MainComponent final : public juce::Component
{
public:
    explicit MainComponent (WebView::PageLoadedCallback onPageLoaded)
        : webView (std::move (onPageLoaded))
    {
        addAndMakeVisible (webView);
    }

    WebView& getWebView() noexcept { return webView; }

    void paint (juce::Graphics& g) override
    {
        // Shown for the moment before the web view paints its first frame.
        g.fillAll (juce::Colour (0xff101014));
    }

    void resized() override
    {
        webView.setBounds (getLocalBounds());
    }

private:
    WebView webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, WebView::PageLoadedCallback onPageLoaded)
        : juce::DocumentWindow (name,
                                juce::Colour (0xff101014),
                                juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (std::move (onPageLoaded)), true);

        setResizable (true, false);
        setResizeLimits (480, 320, 8192, 8192);
        centreWithSize (kWindowWidth, kWindowHeight);
        setVisible (true);
    }

    MainComponent& getMainComponent() noexcept
    {
        return *static_cast<MainComponent*> (getContentComponent());
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};
} // namespace

class InstrumentHubApplication final : public juce::JUCEApplication,
                                       private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise (const juce::String& commandLine) override
    {
        uiCheckMode = commandLine.contains ("--ui-check");
        selftestMode = commandLine.contains ("--selftest");

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), [this] (const juce::String& url)
        {
            lastLoadedUrl = url;
            std::cout << "webview: loaded " << url << std::endl;
        });

        if (uiCheckMode)
        {
            uiCheckDeadline = juce::Time::getMillisecondCounterHiRes() + kUiCheckTimeoutMs;
            startTimer (200);
        }
        else if (selftestMode)
        {
            std::cout << "selftest: window opened " << kWindowWidth << "x" << kWindowHeight
                      << ", quitting in 3 s" << std::endl;
            startTimer (3000);
        }
    }

    void shutdown() override
    {
        stopTimer();
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    void timerCallback() override
    {
        if (uiCheckMode)
        {
            checkUi();
            return;
        }

        stopTimer();

        if (mainWindow == nullptr || ! mainWindow->isOnDesktop() || ! mainWindow->isVisible())
        {
            std::cerr << "selftest: FAILED, window is not on the desktop" << std::endl;
            setApplicationReturnValue (1);
            quit();
            return;
        }

        const auto bounds = mainWindow->getScreenBounds();
        std::cout << "selftest: window on desktop at " << bounds.getWidth() << "x"
                  << bounds.getHeight() << ", quitting cleanly" << std::endl;
        quit();
    }

    void checkUi()
    {
        if (mainWindow == nullptr)
            return;

        if (juce::Time::getMillisecondCounterHiRes() > uiCheckDeadline)
        {
            stopTimer();
            std::cerr << "ui-check: FAILED, heading not found after "
                      << static_cast<int> (kUiCheckTimeoutMs) << " ms"
                      << " (last heading: \"" << lastHeading << "\""
                      << ", url: " << lastLoadedUrl << ")" << std::endl;
            setApplicationReturnValue (1);
            quit();
            return;
        }

        if (evaluationInFlight)
            return;

        evaluationInFlight = true;

        mainWindow->getMainComponent().getWebView().evaluateJavascript (
            "(() => {"
            "  const el = document.querySelector('[data-testid=\"app-title\"]');"
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
                        stopTimer();

                        std::cout << "ui-check: PASS heading=\"" << heading << "\""
                                  << " url=" << lastLoadedUrl
                                  << " mode="
                                  << (WebView::isDevelopmentBuild() ? "debug-dev-server"
                                                                    : "release-embedded")
                                  << std::endl;

                        setApplicationReturnValue (0);
                        quit();
                    }
                }
            });
    }

    std::unique_ptr<MainWindow> mainWindow;
    bool uiCheckMode = false;
    bool selftestMode = false;
    bool uiCheckPassed = false;
    bool evaluationInFlight = false;
    double uiCheckDeadline = 0.0;
    juce::String lastLoadedUrl;
    juce::String lastHeading;
};

START_JUCE_APPLICATION (InstrumentHubApplication)
