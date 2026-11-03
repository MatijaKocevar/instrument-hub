#include "Bridge/WebView.h"
#include "Session.h"
#include "app/CommandLine.h"
#include "app/Constants.h"
#include "app/ControlApi.h"
#include "app/ControllerSurface.h"
#include "app/DiagnosticModes.h"
#include "app/Modes.h"
#include "app/ProjectSetup.h"
#include "app/SessionSetup.h"
#include "app/UiModes.h"
#include "diag/CrashDump.h"
#include "diag/Diagnostics.h"
#include "plugins/ProcessLimits.h"
#include "settings/SettingsStore.h"

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
}

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

        ihub::diag::installJuceLoggerSink();

        ihub::diag::CrashDumpOptions crashOptions;
        crashOptions.path = ihub::app::getCommandLineValue (commandLine, "--crash-dump")
                                .toStdString();
        ihub::diag::installCrashDump (crashOptions);

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
                setApplicationReturnValue (ihub::app::runListDevices (commandLine));
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

        juce::File projectFile;
        Api::AudioRestoreState audioRestore;

        settingsStore = std::make_unique<ihub::settings::SettingsStore>();

        if (const auto status = settingsStore->load();
            status != ihub::settings::LoadStatus::loaded
            && status != ihub::settings::LoadStatus::missing)
        {
            std::cerr << "settings: " << settingsStore->getLastError() << std::endl;
        }

        if (mode == ihub::app::AppMode::gui || mode == ihub::app::AppMode::serve)
        {
            projectFile = ihub::app::resolveProjectFile (commandLine);
            auto startup = ihub::app::startSession (commandLine,
                                                    projectFile,
                                                    settingsStore.get());
            audioRestore = startup.audioRestore;
            session = std::move (startup.session);
        }

        const bool apiRequired = mode == ihub::app::AppMode::serve;

        if (! controlApi.start (commandLine,
                                session.get(),
                                apiRequired,
                                projectFile,
                                [this] { controller.refreshLeds(); },
                                settingsStore.get(),
                                audioRestore))
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

        if (mode == ihub::app::AppMode::gui)
            controller.start (controlApi.getState(),
                              controlApi.getMidiBridge(),
                              projectFile);

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
            ledPollMode = true;
            startTimer (100);
        }
    }

    void shutdown() override
    {
        stopTimer();
        uiMode.reset();

        controller.resetMapping();

        controlApi.stopServer();
        controlApi.flushProject();
        controlApi.detachMidi();
        controller.shutdown();
        controlApi.resetServer();
        controlApi.resetAudio();

        if (session != nullptr)
        {
            session->beginShutdown();
            session->closeDevice();
            session.reset();
        }

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
    std::unique_ptr<ihub::settings::SettingsStore> settingsStore;
    ihub::app::ControllerSurface controller;
    ihub::app::ControlApi controlApi;
    std::unique_ptr<ihub::app::UiModeController> uiMode;
    bool ledPollMode = false;
};

START_JUCE_APPLICATION (InstrumentHubApplication)
