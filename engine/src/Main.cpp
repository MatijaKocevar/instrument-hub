#include "Bridge/WebView.h"
#include "Session.h"
#include "api/PluginBridge.h"
#include "api/Server.h"
#include "api/SessionBridge.h"
#include "audio/AudioEngine.h"
#include "audio/AudioSettings.h"
#include "audio/DeviceManager.h"
#include "audio/RtGuard.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"
#include "diag/CrashDump.h"
#include "diag/Diagnostics.h"
#include "diag/LogRing.h"
#include "plugins/ProcessLimits.h"
#include "plugins/PluginInventory.h"
#include "plugins/Scanner.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#if JUCE_LINUX
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

namespace
{
constexpr int kWindowWidth = 1200;
constexpr int kWindowHeight = 800;
constexpr int kSnapshotWidth = 1200;
constexpr int kSnapshotHeight = 800;
constexpr double kUiCheckTimeoutMs = 20000.0;
constexpr double kSnapshotTimeoutMs = 30000.0;
constexpr double kSnapshotSettleMs = 400.0;

/** Number of distinct colours in a coarse grid of an image.

    A snapshot of the real UI has many (text antialiasing alone produces
    hundreds); a blank or fallback-coloured component has one or two.
*/
int countDistinctColours (const juce::Image& image)
{
    if (image.isNull() || image.getWidth() <= 0 || image.getHeight() <= 0)
        return 0;

    std::set<juce::uint32> colours;
    constexpr int steps = 64;

    for (int y = 0; y < steps; ++y)
    {
        const int py = juce::jlimit (0, image.getHeight() - 1,
                                     y * image.getHeight() / steps);

        for (int x = 0; x < steps; ++x)
        {
            const int px = juce::jlimit (0, image.getWidth() - 1,
                                         x * image.getWidth() / steps);
            colours.insert (image.getPixelAt (px, py).getARGB());
        }
    }

    return static_cast<int> (colours.size());
}

/** Number of sampled pixels that are clearly brighter than the dark chrome.

    The app background is a dark gradient (#101014 with two faint radial
    washes). The title, labels and panels paint near-white pixels, so a
    background-only frame has none while a real render has thousands.
*/
int countBrightPixels (const juce::Image& image)
{
    if (image.isNull() || image.getWidth() <= 0 || image.getHeight() <= 0)
        return 0;

    int bright = 0;
    constexpr int steps = 64;

    for (int y = 0; y < steps; ++y)
    {
        const int py = juce::jlimit (0, image.getHeight() - 1,
                                     y * image.getHeight() / steps);

        for (int x = 0; x < steps; ++x)
        {
            const int px = juce::jlimit (0, image.getWidth() - 1,
                                         x * image.getWidth() / steps);

            if (image.getPixelAt (px, py).getPerceivedBrightness() > 0.55f)
                ++bright;
        }
    }

    return bright;
}

/** True when an image contains rendered UI, not just a flat/gradient fill. */
bool looksRendered (const juce::Image& image)
{
    return countDistinctColours (image) >= 8
           && countBrightPixels (image) >= 16;
}

#if JUCE_LINUX
/** Expands a TrueColor pixel component to 8 bits. */
juce::uint8 maskToByte (unsigned long pixel, unsigned long mask)
{
    if (mask == 0)
        return 0;

    int shift = 0;

    while (((mask >> shift) & 1UL) == 0)
        ++shift;

    const auto max = mask >> shift;
    const auto value = (pixel & mask) >> shift;

    return static_cast<juce::uint8> ((value * 255 + max / 2) / max);
}

/** Captures the native window backing a JUCE component.

    On Linux the web view is a separate GTK/WebKit process embedded through
    XEmbed, so `createComponentSnapshot()` only sees the JUCE fallback fill.
    Reading the peer window straight from the X server captures the real,
    already-rendered UI without shelling out to an external screenshot tool.
*/
juce::Image captureNativeWindow (juce::Component& component)
{
    auto* peer = component.getPeer();

    if (peer == nullptr)
        return {};

    auto* display = XOpenDisplay (nullptr);

    if (display == nullptr)
        return {};

    const auto window = static_cast<Window> (reinterpret_cast<juce::pointer_sized_uint> (peer->getNativeHandle()));
    XWindowAttributes attributes{};

    if (XGetWindowAttributes (display, window, &attributes) == 0
        || attributes.width <= 0 || attributes.height <= 0)
    {
        XCloseDisplay (display);
        return {};
    }

    // The web view paints underneath the window manager, but a raise and a
    // sync before reading makes the result deterministic on a shared desktop.
    XRaiseWindow (display, window);
    XSync (display, False);

    Window child = 0;
    int rootX = 0;
    int rootY = 0;
    XTranslateCoordinates (display, window, DefaultRootWindow (display),
                           0, 0, &rootX, &rootY, &child);

    const auto width = static_cast<unsigned int> (attributes.width);
    const auto height = static_cast<unsigned int> (attributes.height);

    // Reading the window itself includes the XEmbedded WebKit child window and
    // is what desktop grabbers see. The root fallback covers window managers
    // that draw the client into an off-screen frame.
    XImage* source = XGetImage (display, window, 0, 0, width, height,
                                AllPlanes, ZPixmap);

    if (source == nullptr)
        source = XGetImage (display, DefaultRootWindow (display),
                            rootX, rootY, width, height, AllPlanes, ZPixmap);

    if (source == nullptr)
    {
        XCloseDisplay (display);
        return {};
    }

    juce::Image image (juce::Image::ARGB, attributes.width, attributes.height, true);

    for (int y = 0; y < attributes.height; ++y)
    {
        for (int x = 0; x < attributes.width; ++x)
        {
            const auto pixel = XGetPixel (source, x, y);
            const auto red = maskToByte (pixel, source->red_mask);
            const auto green = maskToByte (pixel, source->green_mask);
            const auto blue = maskToByte (pixel, source->blue_mask);

            image.setPixelAt (x, y, juce::Colour (red, green, blue));
        }
    }

    XDestroyImage (source);
    XCloseDisplay (display);

    return image;
}
#else
juce::Image captureNativeWindow (juce::Component&)
{
    return {};
}
#endif

/** Captures the live web view.

    On Linux the page is rendered by an out-of-process GTK/WebKit fork that is
    embedded with XEmbed, so `createComponentSnapshot()` only sees the blank
    JUCE fallback. The native window is therefore the primary source and the
    component snapshot is only the fallback (other platforms, or a window that
    has not been mapped yet).
*/
juce::Image captureSnapshot (juce::Component& component)
{
    auto nativeImage = captureNativeWindow (component);

    if (looksRendered (nativeImage))
        return nativeImage;

    auto componentImage = component.createComponentSnapshot (component.getLocalBounds());

    if (looksRendered (componentImage))
        return componentImage;

    return nativeImage.isNull() ? componentImage : nativeImage;
}

bool writeSnapshot (const juce::Image& image, const juce::File& file)
{
    file.getParentDirectory().createDirectory();

    auto stream = file.createOutputStream();

    if (stream == nullptr || ! stream->openedOk())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, *stream);
}

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

//==============================================================================
// --diagnostics (T111)
//
// Opens the ALSA Loopback through the real device path, renders silence
// through the fixed graph for a short, bounded time, then prints one report
// covering the device, buffer, latency, xruns, loaded plugins and the newest
// structured log records. The same report can be written to a file with
// --diagnostics-out <path>.
int runDiagnostics (const juce::String& commandLine)
{
    using namespace ihub::diag;

    const auto durationValue = getCommandLineValue (commandLine, "--diagnostics-ms");

    int durationMs = durationValue.isNotEmpty() ? durationValue.getIntValue() : 1000;
    durationMs = juce::jlimit (100, 20000, durationMs);

    juce::File pluginList = ihub::plugins::Scanner::defaultReportFile();

    if (const auto pluginDataDir = getCommandLineValue (commandLine, "--plugin-data-dir");
        pluginDataDir.isNotEmpty())
    {
        pluginList = juce::File (pluginDataDir)
                         .getChildFile (ihub::plugins::Scanner::reportFileName);
    }

    auto& ring = defaultLog();

    DiagnosticsOptions options;
    options.knownPluginListFile = pluginList;
    options.crashDumpPath = getCrashDumpPath();
    options.maxLogRecords = 128;

    ring.log (LogLevel::info, LogSource::diagnostics, 100,
              "diagnostics started, crash dump " + getCrashDumpPath());

    AudioEngine::DeviceManager devices;
    AudioEngine::DeviceSetup setup;

    bool found = false;

    for (const auto& info : devices.getDevices())
    {
        if (! info.hasOutput || ! info.name.containsIgnoreCase ("Loopback"))
            continue;

        setup.deviceTypeName = info.typeName;
        setup.outputDeviceName = info.name;
        setup.inputDeviceName = info.hasInput ? info.name : juce::String();
        setup.sampleRate = 48000.0;
        setup.bufferSize = 128;
        setup.numOutputChannels = juce::jmin (2, info.outputChannelNames.size());
        setup.numInputChannels = info.hasInput
                                     ? juce::jmin (2, info.inputChannelNames.size())
                                     : 0;
        setup.outputChannels = AudioEngine::DeviceSetup::channelMask (
            setup.numOutputChannels);
        setup.inputChannels = AudioEngine::DeviceSetup::channelMask (
            setup.numInputChannels);
        found = true;
        break;
    }

    AudioEngine::AudioEngine engine;

    if (! found)
    {
        ring.log (LogLevel::error, LogSource::diagnostics, 110,
                  "no Loopback output device was enumerated");
    }
    else
    {
        juce::String error;

        if (! devices.open (setup, &error))
        {
            ring.log (LogLevel::error, LogSource::diagnostics, 111,
                      "could not open Loopback: " + error.toStdString());
        }
        else
        {
            ring.log (LogLevel::info, LogSource::diagnostics, 101,
                      "opened " + setup.outputDeviceName.toStdString() + " at "
                          + juce::String (juce::roundToInt (devices.getCurrentSampleRate()))
                                .toStdString()
                          + " Hz / "
                          + juce::String (devices.getCurrentBufferSize()).toStdString()
                          + " frames");

            // The engine is the device callback: prepare() runs from
            // audioDeviceAboutToStart() and every block feeds TimingMonitor.
            devices.setAudioCallback (&engine);

            juce::Thread::sleep (durationMs);

            devices.setAudioCallback (nullptr);

            const auto snapshotTiming = engine.getTimingMonitor().snapshot();

            ring.log (LogLevel::info, LogSource::diagnostics, 102,
                      "rendered " + std::to_string (snapshotTiming.callbacks)
                          + " callbacks in " + std::to_string (durationMs)
                          + " ms, " + std::to_string (snapshotTiming.overruns)
                          + " callback xruns");
        }
    }

    const auto timing = engine.getTimingMonitor().snapshot();
    const std::vector<PluginDiagnostics> loadedPlugins;
    const auto snapshot = collectDiagnostics (devices, timing, loadedPlugins, ring, options);

    const auto report = formatDiagnostics (snapshot);
    std::cout << report;
    std::cout.flush();

    if (const auto outPath = getCommandLineValue (commandLine, "--diagnostics-out");
        outPath.isNotEmpty())
    {
        if (! writeDiagnostics (snapshot, juce::File (outPath)))
            std::cerr << "diagnostics: could not write " << outPath << std::endl;
    }

    return snapshot.device.open ? 0 : 1;
}

//==============================================================================
// --session-smoke (T114)
//
// Composes the default session, renders it offline to measure the armed slot's
// RMS and the discontinuity across a slot switch, then opens the real ALSA
// Loopback and streams the same graph for a short bounded time. The offline
// pass is deterministic (no wall-clock timing); the device pass only proves
// that the callback is running and that the graph really reaches hardware.

int runSessionSmoke (const juce::String& commandLine)
{
    constexpr double switchLimitDbfs = -60.0;
    constexpr double minimumRmsDbfs = -60.0;

    ihub::SessionConfig config;
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.device.preferLoopback = true;

    if (const auto value = getCommandLineValue (commandLine, "--audio-device"); value.isNotEmpty())
        config.device.outputDeviceName = value;

    if (const auto value = getCommandLineValue (commandLine, "--audio-sample-rate"); value.isNotEmpty())
        config.device.sampleRate = value.getDoubleValue();

    if (const auto value = getCommandLineValue (commandLine, "--audio-buffer-size"); value.isNotEmpty())
        config.device.bufferSize = value.getIntValue();

    ihub::Session session;
    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "session-smoke: FAILED to build the default set: "
                  << error << std::endl;
        return 1;
    }

    std::cout << "session-smoke: built " << session.getNumSlots()
              << " slots" << std::endl;

    const auto report = session.measureSwitch (0, 1);

    if (! report.rendered)
    {
        std::cerr << "session-smoke: FAILED, the switch render produced no samples"
                  << std::endl;
        return 1;
    }

    std::cout << "session-smoke: armed_slot=" << report.fromSlot
              << " rms=" << report.rms
              << " rms_dbfs=" << report.rmsDbfs
              << " switch_to=" << report.toSlot
              << " discontinuity_dbfs=" << report.discontinuityDbfs
              << " max_slew_dbfs=" << report.maxSampleDeltaDbfs
              << " samples=" << report.samplesMeasured
              << std::endl;

    const bool rmsOk = report.rms > 0.0 && report.rmsDbfs > minimumRmsDbfs;
    const bool switchOk = report.discontinuityDbfs <= switchLimitDbfs;

    if (! rmsOk)
        std::cerr << "session-smoke: FAILED, armed slot RMS is zero/silent ("
                  << report.rmsDbfs << " dBFS)" << std::endl;

    if (! switchOk)
        std::cerr << "session-smoke: FAILED, switch discontinuity "
                  << report.discontinuityDbfs << " dBFS is above "
                  << switchLimitDbfs << " dBFS" << std::endl;

    if (! session.openDevice (&error))
    {
        std::cerr << "session-smoke: FAILED to open the ALSA Loopback: "
                  << error << std::endl;
        return 1;
    }

    const auto deviceName = session.getDevices().getCurrentDeviceName();
    const auto deviceRate = session.getDevices().getCurrentSampleRate();
    const auto deviceBlock = session.getDevices().getCurrentBufferSize();

    session.armSlot (0);
    juce::Thread::sleep (300);

    const auto timing = session.getEngine().getTimingMonitor().snapshot();
    session.closeDevice();

    const bool deviceOk = timing.callbacks > 0;

    std::cout << "session-smoke: device=\"" << deviceName << "\""
              << " rate=" << deviceRate
              << " block=" << deviceBlock
              << " callbacks=" << timing.callbacks
              << " " << (deviceOk ? "callback-running" : "callback-silent")
              << std::endl;

    const bool passed = rmsOk && switchOk && deviceOk;

    std::cout << "session-smoke: "
              << (passed ? "PASS" : "FAIL")
              << " (rms>" << minimumRmsDbfs << "dBFS, discontinuity<="
              << switchLimitDbfs << "dBFS, device callback running)"
              << std::endl;

    return passed ? 0 : 1;
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

        // The snapshot contract is a 1200x800 image. Sizing the *content* (not
        // the decorated window) means the web view really is that size.
        setContentComponentSize (kWindowWidth, kWindowHeight);
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
        crashOptions.path = getCommandLineValue (commandLine, "--crash-dump").toStdString();
        ihub::diag::installCrashDump (crashOptions);

        uiCheckMode = commandLine.contains ("--ui-check");
        selftestMode = commandLine.contains ("--selftest");
        serveMode = commandLine.contains ("--serve");
        snapshotMode = commandLine.contains ("--snapshot");

        if (snapshotMode)
        {
            const auto path = getCommandLineValue (commandLine, "--snapshot");

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

        if (commandLine.contains ("--diagnostics"))
        {
            setApplicationReturnValue (runDiagnostics (commandLine));
            quit();
            return;
        }

        if (commandLine.contains ("--session-smoke"))
        {
            setApplicationReturnValue (runSessionSmoke (commandLine));
            quit();
            return;
        }

        // Compose the audio session for the normal application and for
        // --serve. Diagnostic and UI-only modes quit above or own no audio.
        if (! snapshotMode && ! uiCheckMode && ! selftestMode)
            startSession (commandLine);

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

        startController();

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), [this] (const juce::String& url)
        {
            lastLoadedUrl = url;
            snapshotPageLoaded = true;
            std::cout << "webview: loaded " << url << std::endl;
        });

        if (snapshotMode)
        {
            snapshotDeadline = juce::Time::getMillisecondCounterHiRes() + kSnapshotTimeoutMs;
            startTimer (100);
        }
        else if (uiCheckMode)
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

        if (launchpad != nullptr)
        {
            launchpad->disconnect();
            launchpad.reset();
        }

        mapping.reset();

        if (apiServer != nullptr)
            apiServer->stop();

        apiServer.reset();

        // Stop the device before destroying the graph it renders.
        if (session != nullptr)
        {
            session->closeDevice();
            session.reset();
        }

        audioSettings.reset();
        apiState.reset();
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    /**
        Builds and opens the composed audio session.

        The graph is created once: an AudioEngine with eight fixed slots, a
        SlotSwitcher over them, and the default set created through the
        existing instrument factories. The configured device is then opened and
        the session becomes its callback. Nothing is rewired at runtime;
        arming a slot only publishes a lock-free crossfade request.
    */
    void startSession (const juce::String& commandLine)
    {
        if (commandLine.contains ("--no-audio"))
        {
            std::cout << "session: audio disabled by --no-audio" << std::endl;
            return;
        }

        ihub::SessionConfig config;
        config.slots = ihub::Session::makeDefaultSet();

        // Give every pad its real family/patch identity from the shipped set
        // file. The audio sources stay initialisation tones until a set with
        // sample kits or plugin bundles is loaded, but `/state` already
        // mirrors the 8 x 8 tree the Launchpad lights up.
        const auto setFile = Controller::Mapping::findDefaultSetFile();

        if (! setFile.empty())
        {
            Controller::SetMapping document;
            Controller::MappingError mappingError;

            if (Controller::loadSetMappingFile (setFile, document, mappingError))
            {
                for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
                {
                    const auto* binding = document.padBinding (pad);
                    const auto* instrument = binding != nullptr
                                                 ? document.instrumentFor (*binding)
                                                 : nullptr;

                    if (binding == nullptr || instrument == nullptr)
                        continue;

                    const int slot = instrument->slot >= 0 ? instrument->slot
                                                           : binding->slot;

                    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
                        continue;

                    auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
                    slotConfig.name = instrument->name;
                    slotConfig.patch = instrument->id;
                    slotConfig.patchName = instrument->name;
                    slotConfig.family = binding->family;
                    slotConfig.familyName = binding->family;

                    // Persisted mix/colour/descriptor (T119). The audio source
                    // stays an init tone until a real kit or plugin is loaded,
                    // but the API publishes the set's full patch tree.
                    slotConfig.gain = instrument->volumeDb <= -60.0f
                                          ? 0.0f
                                          : std::pow (10.0f, instrument->volumeDb / 20.0f);
                    slotConfig.pan = instrument->pan;
                    slotConfig.muted = instrument->muted;
                    slotConfig.solo = instrument->solo;
                    slotConfig.colorIndex = instrument->resolvedColorIndex;
                    slotConfig.instrumentKind = instrument->kind;
                    slotConfig.pluginFormat = instrument->pluginFormat;
                    slotConfig.pluginId = instrument->pluginId;
                    slotConfig.pluginName = instrument->pluginName;
                    slotConfig.pluginManufacturer = instrument->pluginManufacturer;
                    slotConfig.pluginVersion = instrument->pluginVersion;
                    slotConfig.samplePath = instrument->samplePath;

                    // Persisted insert chain (T119/T121). A built-in's id is
                    // its type; a plugin entry carries the bundle path in id.
                    for (const auto& fxSpec : instrument->fxChain)
                    {
                        AudioEngine::FxInsertConfig fx;
                        fx.bypassed = fxSpec.bypassed;
                        fx.id = fxSpec.id;
                        fx.name = fxSpec.name;

                        if (fxSpec.kind == "plugin")
                        {
                            fx.type = AudioEngine::FxInsertType::vst3;
                            fx.path = fxSpec.id;
                        }
                        else if (! AudioEngine::fxInsertTypeFromString (fxSpec.id, fx.type))
                        {
                            continue;
                        }

                        for (const auto& [key, value] : fxSpec.parameters)
                        {
                            if (key == "wet")
                                fx.wet = juce::jlimit (0.0f, 1.0f, value);
                            else
                                fx.parameters[key] = value;
                        }

                        slotConfig.fxChain.push_back (std::move (fx));
                    }

                    if (const auto family = document.families.find (binding->family);
                        family != document.families.end())
                    {
                        slotConfig.familyName = family->second.name;
                    }
                }
            }
            else
            {
                std::cerr << "session: could not read " << setFile.string()
                          << ": " << mappingError.message << std::endl;
            }
        }

        if (const auto type = getCommandLineValue (commandLine, "--audio-type"); type.isNotEmpty())
            config.device.deviceTypeName = type;

        if (const auto name = getCommandLineValue (commandLine, "--audio-device"); name.isNotEmpty())
            config.device.outputDeviceName = name;

        if (const auto rate = getCommandLineValue (commandLine, "--audio-sample-rate"); rate.isNotEmpty())
            config.device.sampleRate = rate.getDoubleValue();

        if (const auto block = getCommandLineValue (commandLine, "--audio-buffer-size"); block.isNotEmpty())
            config.device.bufferSize = block.getIntValue();

        config.device.numOutputChannels = 2;
        config.device.numInputChannels = 2;

        session = std::make_unique<ihub::Session>();
        juce::String error;

        if (! session->build (config, error))
        {
            std::cerr << "session: could not build the default set: " << error << std::endl;
            session.reset();
            return;
        }

        if (! session->openDevice (&error))
        {
            std::cerr << "session: audio is not running: " << error << std::endl;
        }
        else
        {
            std::cout << "session: opened " << session->getDevices().getCurrentDeviceName()
                      << " at " << session->getDevices().getCurrentSampleRate()
                      << " Hz / " << session->getDevices().getCurrentBufferSize()
                      << " frames, " << session->getNumSlots()
                      << " slots, default set loaded" << std::endl;
        }

        // The default set is playable immediately. The control API mirrors it
        // and routes arm requests into the switcher once the state store is
        // created below.
        session->armSlot (0);
    }

    /**
        Starts the Launchpad and the set.yaml mapping.

        Pads arm the slot their binding names, the API state mirrors that
        selection for the UI, and the LED of the selected pad turns green while
        every other pad goes dark. Diagnostic and snapshot modes skip this so a
        test run never switches a connected controller out of Live mode.
    */
    void startController()
    {
        if (snapshotMode || uiCheckMode || selftestMode)
            return;

        const auto setFile = Controller::Mapping::findDefaultSetFile();

        if (! setFile.empty())
        {
            mapping = std::make_unique<Controller::Mapping>();

            Controller::Mapping::Listeners mappingListeners;

            mappingListeners.armSlot = [this] (int slot)
            {
                if (slot >= 0 && apiState != nullptr)
                    (void) apiState->setArmedSlot (slot);
            };

            mappingListeners.padEvent = [this] (int pad,
                                                const Controller::PadBinding*,
                                                bool pressed)
            {
                if (! pressed || launchpad == nullptr)
                    return;

                for (int index = 0; index < Launchpad::padCount; ++index)
                    launchpad->setPadState (index,
                                            index == pad ? Controller::PadState::active
                                                         : Controller::PadState::idle);
            };

            // The top row selects a macro page, the right column a scene. With
            // no encoders these LEDs are the only feedback the surface gives
            // for those selections.
            mappingListeners.bankEvent = [this] (int bank,
                                                 const Controller::MacroBankSpec*)
            {
                if (launchpad == nullptr)
                    return;

                for (int index = 0; index < Launchpad::topRowCount; ++index)
                    launchpad->setButtonState (
                        Launchpad::topRowCCForIndex (index),
                        index == bank ? Controller::PadState::active
                                      : Controller::PadState::idle);
            };

            mappingListeners.sceneEvent = [this] (int scene,
                                                  const Controller::SceneSpec*)
            {
                if (launchpad == nullptr)
                    return;

                for (int index = 0; index < Launchpad::rightColumnCount; ++index)
                    launchpad->setButtonState (
                        Launchpad::rightColumnCCForIndex (index),
                        index == scene ? Controller::PadState::active
                                       : Controller::PadState::idle);
            };

            mapping->setListeners (std::move (mappingListeners));

            Controller::MappingError error;

            if (! mapping->load (setFile, &error))
                std::cerr << "mapping: " << error.message << std::endl;
            else
                mapping->startWatching();
        }

        launchpad = std::make_unique<Controller::LaunchpadDriver>();

        Controller::LaunchpadDriver::DriverListeners driverListeners;

        driverListeners.padEvent = [this] (int pad, bool pressed)
        {
            if (mapping != nullptr)
                mapping->handlePad (pad, pressed);
        };

        driverListeners.buttonEvent = [this] (int cc, bool pressed)
        {
            if (mapping != nullptr)
                mapping->handleButton (cc, pressed);
        };

        launchpad->setListeners (std::move (driverListeners));

        if (! launchpad->connect())
            std::cerr << "controller: " << launchpad->getLastError() << std::endl;
        else
            launchpad->clearLeds();
    }

    bool startControlApi (const juce::String& commandLine, bool required)
    {
        Api::Server::Options options;

        if (const auto port = getCommandLineValue (commandLine, "--api-port"); port.isNotEmpty())
            options.port = port.getIntValue();

        if (const auto address = getCommandLineValue (commandLine, "--api-host"); address.isNotEmpty())
            options.address = address.toStdString();

        options.authToken = getCommandLineValue (commandLine, "--api-token").toStdString();
        options.lanMode = commandLine.contains ("--lan") || commandLine.contains ("--lan-mode");

        const int apiSlots = session != nullptr ? session->getNumSlots()
                                                : ihub::Session::defaultLogicalSlots;
        apiState = std::make_unique<Api::StateStore> (apiSlots);
        apiServer = std::make_unique<Api::Server> (*apiState, options);
        pluginInventory = std::make_unique<ihub::plugins::PluginInventory>();
        Api::attachPluginInventory (*apiServer, *pluginInventory);

        const auto inventory = pluginInventory->snapshot();

        if (inventory.scanned)
        {
            std::cout << "plugins: " << inventory.total
                      << " installed VST3 type(s) in the inventory" << std::endl;
        }
        else if (pluginInventory->requestRescan())
        {
            std::cout << "plugins: scanning VST3 directories in the background"
                      << std::endl;
        }

        audioSettings = std::make_unique<AudioEngine::AudioSettingsBridge>();

        // The session is the single owner of the audio device. Wire the API to
        // it: arm requests reach SlotSwitcher, transport commands drive the
        // session transport, /state reports the live slot states and /meters
        // reads the real slot and master peaks.
        if (session != nullptr)
        {
            Api::attachSession (*apiState, *apiServer, *session);
            audioSettings->setTimingMonitor (&session->getEngine().getTimingMonitor());
        }

        if (session == nullptr
            && (commandLine.contains ("--audio")
                || commandLine.contains ("--audio-loopback")))
        {
            juce::String audioError;

            if (audioSettings->openLoopback (&audioError))
            {
                std::cout << "audio: opened Loopback at "
                          << audioSettings->snapshot().sampleRate << " Hz / "
                          << audioSettings->snapshot().bufferSize << " frames"
                          << std::endl;
            }
            else
            {
                std::cerr << "audio: could not open the Loopback device: "
                          << audioError << std::endl;
            }
        }

        apiServer->setAudioHandlers (
            [this] { return audioSettings->snapshot(); },
            [this] (const Api::AudioSettingsPatch& patch, std::string& error)
            {
                return audioSettings->apply (patch, error);
            },
            [this] { return audioSettings->measure(); });

        if (! apiServer->start())
        {
            std::cerr << "api: FAILED to bind " << options.address << ":" << options.port
                      << (apiServer->hasAuthToken() ? " (auth enabled)" : "")
                      << std::endl;
            apiServer.reset();
            audioSettings.reset();
            apiState.reset();

            if (! required)
                std::cerr << "api: continuing without the control server" << std::endl;

            return false;
        }

        std::cout << "api: listening on http://" << apiServer->getAddress() << ":"
                  << apiServer->getPort()
                  << " meters=" << options.meterHz << "Hz"
                  << " auth=" << (apiServer->hasAuthToken() ? "enabled" : "disabled")
                  << " lan=" << (apiServer->isLanModeEnabled() ? "enabled" : "disabled")
                  << std::endl;

        return true;
    }

    void timerCallback() override
    {
        if (snapshotMode)
        {
            checkSnapshot();
            return;
        }

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

    //==============================================================================
    // --snapshot <path> (T054)
    //
    // Waits until the web view has actually rendered the React tree (not just
    // fired a load callback), then writes a 1200x800 PNG from inside the
    // process. A snapshot is only accepted when it is the right size and has
    // real image content, so a blank/fallback frame cannot masquerade as
    // success.

    void failSnapshot (const juce::String& reason)
    {
        stopTimer();
        std::cerr << "snapshot: FAILED, " << reason
                  << " (url: " << lastLoadedUrl << ")" << std::endl;
        setApplicationReturnValue (1);
        quit();
    }

    void checkSnapshot()
    {
        if (mainWindow == nullptr)
            return;

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

            mainWindow->getMainComponent().getWebView().evaluateJavascript (
                "(() => {"
                "  const el = document.querySelector('[data-testid=\"app-title\"]');"
                "  return el ? el.textContent.trim() : '';"
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

                    if (heading == "Instrument Hub")
                    {
                        snapshotProbePassed = true;
                        snapshotCaptureAt = juce::Time::getMillisecondCounterHiRes()
                                            + kSnapshotSettleMs;

                        // Give the compositor a chance to present the frame
                        // before it is read back from the X server.
                        mainWindow->toFront (true);
                    }
                });

            return;
        }

        if (now < snapshotCaptureAt)
            return;

        auto& content = mainWindow->getMainComponent();
        const auto image = captureSnapshot (content);
        const auto colours = countDistinctColours (image);
        const auto bright = countBrightPixels (image);

        // The page paints its background immediately but the React tree lands
        // a frame or two later. Keep sampling until the frame really contains
        // the UI instead of trusting a single fixed delay.
        if (image.isNull()
            || image.getWidth() != kSnapshotWidth
            || image.getHeight() != kSnapshotHeight
            || ! looksRendered (image))
        {
            if (now <= snapshotDeadline)
                return;

            stopTimer();

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

        stopTimer();

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

        setApplicationReturnValue (0);
        quit();
    }

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<ihub::Session> session;
    std::unique_ptr<Controller::Mapping> mapping;
    std::unique_ptr<Controller::LaunchpadDriver> launchpad;
    std::unique_ptr<ihub::plugins::PluginInventory> pluginInventory;
    std::unique_ptr<Api::StateStore> apiState;
    std::unique_ptr<Api::Server> apiServer;
    std::unique_ptr<AudioEngine::AudioSettingsBridge> audioSettings;
    bool uiCheckMode = false;
    bool selftestMode = false;
    bool serveMode = false;
    bool snapshotMode = false;
    bool uiCheckPassed = false;
    bool snapshotPageLoaded = false;
    bool snapshotProbePassed = false;
    bool evaluationInFlight = false;
    double uiCheckDeadline = 0.0;
    double snapshotDeadline = 0.0;
    double snapshotCaptureAt = 0.0;
    juce::File snapshotFile;
    juce::String lastLoadedUrl;
    juce::String lastHeading;
};

START_JUCE_APPLICATION (InstrumentHubApplication)
