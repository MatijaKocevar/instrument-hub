#include "Bridge/WebView.h"
#include "Session.h"
#include "app/CommandLine.h"
#include "app/Constants.h"
#include "app/ControlApi.h"
#include "app/ControllerSurface.h"
#include "app/DiagnosticModes.h"
#include "app/Modes.h"
#include "app/SessionSetup.h"
#include "app/UiModes.h"
#include "diag/CrashDump.h"
#include "diag/Diagnostics.h"
#include "plugins/ProcessLimits.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <iostream>
#include <memory>
#include <utility>

namespace
{
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

        // The snapshot contract is a 1200x800 image. Sizing the *content* (not
        // the decorated window) means the web view really is that size.
        setContentComponentSize (ihub::app::kWindowWidth, ihub::app::kWindowHeight);
        centreWithSize (getWidth(), getHeight());
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
        ihub::plugins::capFileDescriptorLimit();

        // Structured logging and the crash dump are active for every mode.
        // The handler writes the last records with async-signal-safe calls and
        // re-raises, so the process still dies with its original status.
        ihub::diag::installJuceLoggerSink();

        ihub::diag::CrashDumpOptions crashOptions;
        crashOptions.path = ihub::app::getCommandLineValue (commandLine, "--crash-dump")
                                .toStdString();
        ihub::diag::installCrashDump (crashOptions);

        // A missing --snapshot path has always failed before any other mode is
        // considered, even when another mode flag is present.
        juce::File snapshotFile;

        if (commandLine.contains ("--snapshot"))
        {
            const auto path = ihub::app::getCommandLineValue (commandLine, "--snapshot");

            if (path.isEmpty())
            {
                std::cerr << "snapshot: FAILED, --snapshot needs an output path" << std::endl;
                setApplicationReturnValue (1);
                quit();
                return;
            }

            snapshotFile = juce::File (path);

            if (! juce::File::isAbsolutePath (path))
                snapshotFile = juce::File::getCurrentWorkingDirectory().getChildFile (path);
        }

        const auto mode = ihub::app::selectMode (commandLine);

        switch (mode)
        {
            case ihub::app::AppMode::pluginScan:
                setApplicationReturnValue (ihub::app::runPluginScan (commandLine));
                quit();
                return;

            case ihub::app::AppMode::rtStrict:
                setApplicationReturnValue (ihub::app::runRtStrictCheck (commandLine));
                quit();
                return;

            case ihub::app::AppMode::listDevices:
                setApplicationReturnValue (ihub::app::runListDevices());
                quit();
                return;

            case ihub::app::AppMode::measureLatency:
                setApplicationReturnValue (ihub::app::runLatencyMeasurement());
                quit();
                return;

            case ihub::app::AppMode::diagnostics:
                setApplicationReturnValue (ihub::app::runDiagnostics (commandLine));
                quit();
                return;

            case ihub::app::AppMode::sessionSmoke:
                setApplicationReturnValue (ihub::app::runSessionSmoke (commandLine));
                quit();
                return;

            case ihub::app::AppMode::gui:
            case ihub::app::AppMode::serve:
            case ihub::app::AppMode::snapshot:
            case ihub::app::AppMode::uiCheck:
            case ihub::app::AppMode::selftest:
                break;
        }

        // Compose the audio session for the normal application and for
        // --serve. Diagnostic and UI-only modes quit above or own no audio.
        if (mode == ihub::app::AppMode::gui || mode == ihub::app::AppMode::serve)
            session = ihub::app::startSession (commandLine);

        // The control API is part of the normal application, but not of the
        // single-purpose diagnostic modes above (they quit immediately).
        const bool apiRequired = mode == ihub::app::AppMode::serve;

        if (! controlApi.start (commandLine,
                                session.get(),
                                apiRequired,
                                [this] { controller.refreshLeds(); }))
        {
            if (apiRequired)
            {
                setApplicationReturnValue (1);
                quit();
                return;
            }
        }

        if (mode == ihub::app::AppMode::serve)
        {
            std::cout << "serve: control API is running, press Ctrl-C to stop" << std::endl;
            return;
        }

        // Only the normal GUI starts the controller; `serve` returned above
        // and the UI-only modes must never switch a connected controller out
        // of Live mode.
        if (mode == ihub::app::AppMode::gui)
            controller.start (controlApi.getState(), controlApi.getMidiBridge());

        mainWindow = std::make_unique<MainWindow> (getApplicationName(),
            [this] (const juce::String& url)
            {
                if (uiMode != nullptr)
                    uiMode->pageLoaded (url);

                std::cout << "webview: loaded " << url << std::endl;
            });

        if (mode == ihub::app::AppMode::snapshot
            || mode == ihub::app::AppMode::uiCheck
            || mode == ihub::app::AppMode::selftest)
        {
            uiMode = std::make_unique<ihub::app::UiModeController> (
                mode,
                *mainWindow,
                mainWindow->getMainComponent(),
                mainWindow->getMainComponent().getWebView(),
                std::move (snapshotFile),
                [this] (int returnValue)
                {
                    stopTimer();
                    setApplicationReturnValue (returnValue);
                    quit();
                });

            uiMode->start();
            startTimer (uiMode->getTimerIntervalMs());
        }
        else
        {
            // Normal window: keep the Launchpad LEDs in sync with the grid,
            // including colour edits that arrive only through the HTTP API.
            ledPollMode = true;
            startTimer (100);
        }
    }

    void shutdown() override
    {
        stopTimer();
        uiMode.reset();

        controller.resetMapping();

        // No handler may observe the driver after this point, so stop the
        // listener before tearing the pad-test thread and the driver down.
        controlApi.stopServer();
        controlApi.detachMidi();
        controller.shutdown();
        controlApi.resetServer();

        // Stop the device before destroying the graph it renders.
        if (session != nullptr)
        {
            session->closeDevice();
            session.reset();
        }

        controlApi.resetAudio();
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
        if (uiMode != nullptr)
        {
            uiMode->tick();
            return;
        }

        if (ledPollMode)
        {
            controller.refreshLeds();
            return;
        }

        stopTimer();
    }

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<ihub::Session> session;
    ihub::app::ControllerSurface controller;
    ihub::app::ControlApi controlApi;
    std::unique_ptr<ihub::app::UiModeController> uiMode;
    bool ledPollMode = false;
};

START_JUCE_APPLICATION (InstrumentHubApplication)
