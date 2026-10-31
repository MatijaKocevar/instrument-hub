#include "Bridge/WebView.h"
#include "api/Server.h"
#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "audio/RtGuard.h"
#include "plugins/Scanner.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

namespace
{
constexpr int kWindowWidth = 1200;
constexpr int kWindowHeight = 800;
constexpr double kUiCheckTimeoutMs = 20000.0;

/** Returns the value following `option`, or an empty string. Supports both
    `--option value` and `--option=value`.
*/
juce::String getCommandLineValue (const juce::String& commandLine, const juce::String& option)
{
    const auto tokens = juce::StringArray::fromTokens (commandLine, true);

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& token = tokens[i];

        if (token == option)
            return i + 1 < tokens.size() ? tokens[i + 1].unquoted() : juce::String();

        if (token.startsWith (option + "="))
            return token.substring (option.length() + 1).unquoted();
    }

    return {};
}

//==============================================================================
// --rt-strict (T014)

/** Keeps a deliberately injected allocation observable to the optimiser.

    `void* volatile` is a volatile pointer, so assigning to it is an observable
    side effect and the allocation cannot be elided at -O2.
*/
void* volatile allocationEscapeSink = nullptr;

enum class RtInjection
{
    none,
    allocation,
    lock,
    log
};

const char* toString (RtInjection injection) noexcept
{
    switch (injection)
    {
        case RtInjection::none:       return "none";
        case RtInjection::allocation: return "allocation";
        case RtInjection::lock:       return "lock";
        case RtInjection::log:        return "log";
    }

    return "none";
}

/** A readable source that can deliberately break the audio-thread contract.

    The sine keeps the engine on the real processing path; the injection is
    performed at the end of `process()`, exactly where a buggy instrument or
    effect would do it.
*/
class RtProbeSource final : public AudioEngine::SlotSource
{
public:
    explicit RtProbeSource (RtInjection injectionToUse)
        : injection (injectionToUse),
          logText ("rt-guard injected log message")
    {
    }

    void prepare (double newSampleRate, int) override
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        position = 0;
    }

    void setArmed (bool shouldBeArmed) noexcept { armed = shouldBeArmed; }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int samples = buffer.getNumSamples();
        const int channels = buffer.getNumChannels();

        const double phase = juce::MathConstants<double>::twoPi * frequency
                             * static_cast<double> (position) / sampleRate;
        const double increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        for (int i = 0; i < samples; ++i)
        {
            const auto value = static_cast<float> (amplitude
                                                   * std::sin (phase + increment
                                                               * static_cast<double> (i)));

            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer (channel)[i] = value;
        }

        position += samples;

        if (! armed)
            return;

        switch (injection)
        {
            case RtInjection::none:
                break;

            case RtInjection::allocation:
            {
                auto* scratch = new float[8];
                scratch[0] = static_cast<float> (position);

                // Publish the pointer through a volatile sink so the optimiser
                // cannot reduce new/delete to a stack buffer at -O2.
                allocationEscapeSink = scratch;
                delete[] scratch;
                allocationEscapeSink = nullptr;
                break;
            }

            case RtInjection::lock:
            {
                std::mutex mutex;
                const std::lock_guard<std::mutex> lock (mutex);
                break;
            }

            case RtInjection::log:
                juce::Logger::writeToLog (logText);
                break;
        }
    }

private:
    RtInjection injection;
    juce::String logText;

    double sampleRate = 48000.0;
    double frequency = 440.0;
    double amplitude = 0.1;
    std::int64_t position = 0;
    bool armed = false;
};

/** Renders the engine under RtGuard and reports whether it stayed clean.

    In strict mode an injected allocation/lock/log aborts inside the first
    block, which is exactly what the acceptance check expects.
*/
int runRtStrictCheck (const juce::String& commandLine)
{
    if (! AudioEngine::RtGuard::isCompiledIn())
    {
        std::cerr << "rt-strict: FAILED, the realtime guard is compiled out of this "
                     "build (configure with -DIHUB_ENABLE_RT_GUARD=ON)" << std::endl;
        return 2;
    }

    RtInjection injection = RtInjection::none;

    if (commandLine.contains ("--rt-inject-allocation"))
        injection = RtInjection::allocation;
    else if (commandLine.contains ("--rt-inject-lock"))
        injection = RtInjection::lock;
    else if (commandLine.contains ("--rt-inject-log"))
        injection = RtInjection::log;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int measuredBlocks = 200;

    AudioEngine::RtGuard::resetCounters();
    AudioEngine::RtGuard::setEnabled (true);
    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::installLogProbe();

    AudioEngine::AudioEngine engine (2, 2);
    RtProbeSource source (injection);

    engine.getSlot (0).setSource (&source);
    engine.getSlot (0).activate();

    // prepare() is a setup call: allocations here are expected and legal.
    engine.prepare (sampleRate, blockSize, 2);

    juce::AudioBuffer<float> output (2, blockSize);
    juce::MidiBuffer midi;

    // Run the activation ramp with the injection disarmed, then measure from a
    // clean slate. This also warms every lazily initialised path.
    for (int block = 0; block < 4; ++block)
        engine.processBlock (output, midi, blockSize);

    const auto warmupCounters = AudioEngine::RtGuard::getCounters();

    AudioEngine::RtGuard::resetCounters();
    source.setArmed (true);
    AudioEngine::RtGuard::setStrict (true);

    if (injection != RtInjection::none)
        std::cout << "rt-strict: injecting " << toString (injection)
                  << ", expecting an abort" << std::endl;

    for (int block = 0; block < measuredBlocks; ++block)
        engine.processBlock (output, midi, blockSize);

    // Unreachable when an injection aborted in strict mode.
    const auto measuredCounters = AudioEngine::RtGuard::getCounters();
    const bool balanced = AudioEngine::RtGuard::assertNoOpenScopes();
    const int openScopes = AudioEngine::RtGuard::getOpenScopeCount();

    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::uninstallLogProbe();

    const auto total = warmupCounters.total() + measuredCounters.total();
    const bool passed = total == 0 && balanced && openScopes == 0;

    std::cout << "rt-strict: blocks=" << measuredBlocks
              << " warmup_violations=" << warmupCounters.total()
              << " violations=" << measuredCounters.total()
              << " (alloc=" << measuredCounters.allocations
              << " lock=" << measuredCounters.locks
              << " log=" << measuredCounters.logs
              << " scope=" << measuredCounters.scopeErrors << ")"
              << " open_scopes=" << openScopes
              << " " << (passed ? "PASS" : "FAIL") << std::endl;

    return passed ? 0 : 1;
}

//==============================================================================
// --scan-plugins <dir>... (T021)
//
// Runs the out-of-process scanner over the given directories (or the VST3
// default locations when none are given) and writes KnownPluginList.xml to the
// user data directory. A crashing plugin is reported as a failure and the app
// keeps running.
//
// --plugin-data-dir <dir> overrides where KnownPluginList.xml is written,
// which keeps tests and CI out of the real user profile.
int runPluginScan (const juce::String& commandLine)
{
    const auto tokens = juce::StringArray::fromTokens (commandLine, true);
    juce::StringArray directories;
    ihub::plugins::Scanner::Options options;
    bool sawFlag = false;

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& token = tokens[i];

        if (token == "--plugin-data-dir")
        {
            if (i + 1 < tokens.size())
                options.dataDirectory = juce::File (tokens[++i].unquoted());

            continue;
        }

        if (token == "--scan-plugins")
        {
            sawFlag = true;
            continue;
        }

        if (sawFlag && ! token.startsWith ("--"))
            directories.add (token.unquoted());
    }

    options.logSink = [] (const juce::String& line)
    {
        std::cout << line << std::endl;
    };

    ihub::plugins::Scanner scanner (options);
    const auto report = scanner.scan (directories);

    std::cout << "scan-plugins: " << report.filesDiscovered << " discovered, "
              << report.succeeded << " ok, " << report.failed << " failed"
              << " (app survived), report=" << report.reportFile.getFullPathName()
              << (report.wroteReport ? "" : " (NOT written)") << std::endl;

    return report.helperAvailable && report.wroteReport ? 0 : 1;
}

//==============================================================================
int runLatencyMeasurement()
{
    constexpr int runs = 5;

    AudioEngine::AudioEngine engine;
    AudioEngine::LatencyProbeOptions options;
    options.bufferSize = 128;
    options.sampleRate = 48000.0;

    std::vector<int> samples;
    samples.reserve (runs);

    std::cout << "latency: measuring " << runs << " times at "
              << options.bufferSize << " frames / " << options.sampleRate << " Hz"
              << std::endl;

    for (int run = 1; run <= runs; ++run)
    {
        const auto result = engine.measureRoundTripLatency (options);

        if (! result.valid)
        {
            std::cerr << "latency: run " << run << " FAILED: "
                      << result.error.toRawUTF8() << std::endl;
            return 1;
        }

        samples.push_back (result.roundTripSamples);

        std::cout << "latency: run " << run << ": "
                  << result.roundTripSamples << " samples, "
                  << result.roundTripMs << " ms, confidence "
                  << result.confidence << std::endl;
    }

    std::sort (samples.begin(), samples.end());

    const int median = samples[samples.size() / 2];
    const int spread = samples.back() - samples.front();
    const double sampleRate = options.sampleRate;
    const double medianMs = static_cast<double> (median) * 1000.0 / sampleRate;
    const int expectedSamples = options.bufferSize * 2;
    const double expectedMs = static_cast<double> (expectedSamples) * 1000.0 / sampleRate;
    const int tolerance = options.bufferSize;

    const bool withinTolerance = std::abs (median - expectedSamples) <= tolerance;
    const bool consistent = spread <= tolerance;

    std::cout << "latency: median " << median << " samples (" << medianMs << " ms)"
              << " spread " << spread << " samples"
              << " expected " << expectedSamples << " samples (" << expectedMs << " ms)"
              << " tolerance +/-" << tolerance << " samples"
              << " within_tolerance=" << (withinTolerance ? "yes" : "no")
              << " consistent=" << (consistent ? "yes" : "no")
              << std::endl;

    return withinTolerance && consistent ? 0 : 1;
}

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
        serveMode = commandLine.contains ("--serve");

        if (commandLine.contains ("--scan-plugins"))
        {
            setApplicationReturnValue (runPluginScan (commandLine));
            quit();
            return;
        }

        if (commandLine.contains ("--rt-strict"))
        {
            setApplicationReturnValue (runRtStrictCheck (commandLine));
            quit();
            return;
        }

        if (commandLine.contains ("--list-devices"))
        {
            AudioEngine::DeviceManager deviceManager;
            std::cout << deviceManager.describeDevices() << std::endl;
            quit();
            return;
        }

        if (commandLine.contains ("--measure-latency"))
        {
            setApplicationReturnValue (runLatencyMeasurement());
            quit();
            return;
        }

        // The control API is part of the normal application, but not of the
        // single-purpose diagnostic modes above (they quit immediately).
        const bool apiRequired = serveMode;

        if (! startControlApi (commandLine, apiRequired))
        {
            if (apiRequired)
            {
                setApplicationReturnValue (1);
                quit();
                return;
            }
        }

        if (serveMode)
        {
            std::cout << "serve: control API is running, press Ctrl-C to stop" << std::endl;
            return;
        }

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

        if (apiServer != nullptr)
            apiServer->stop();

        apiServer.reset();
        apiState.reset();
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    bool startControlApi (const juce::String& commandLine, bool required)
    {
        Api::Server::Options options;

        if (const auto port = getCommandLineValue (commandLine, "--api-port"); port.isNotEmpty())
            options.port = port.getIntValue();

        if (const auto address = getCommandLineValue (commandLine, "--api-host"); address.isNotEmpty())
            options.address = address.toStdString();

        options.authToken = getCommandLineValue (commandLine, "--api-token").toStdString();

        apiState = std::make_unique<Api::StateStore> (AudioEngine::AudioEngine::defaultNumSlots);
        apiServer = std::make_unique<Api::Server> (*apiState, options);

        if (! apiServer->start())
        {
            std::cerr << "api: FAILED to bind " << options.address << ":" << options.port
                      << (apiServer->hasAuthToken() ? " (auth enabled)" : "")
                      << std::endl;
            apiServer.reset();
            apiState.reset();

            if (! required)
                std::cerr << "api: continuing without the control server" << std::endl;

            return false;
        }

        std::cout << "api: listening on http://" << apiServer->getAddress() << ":"
                  << apiServer->getPort()
                  << " meters=" << options.meterHz << "Hz"
                  << " auth=" << (apiServer->hasAuthToken() ? "enabled" : "disabled")
                  << std::endl;

        return true;
    }

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
    std::unique_ptr<Api::StateStore> apiState;
    std::unique_ptr<Api::Server> apiServer;
    bool uiCheckMode = false;
    bool selftestMode = false;
    bool serveMode = false;
    bool uiCheckPassed = false;
    bool evaluationInFlight = false;
    double uiCheckDeadline = 0.0;
    juce::String lastLoadedUrl;
    juce::String lastHeading;
};

START_JUCE_APPLICATION (InstrumentHubApplication)
