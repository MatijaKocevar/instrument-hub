#include "DiagnosticModes.h"

#include "CommandLine.h"

#include "Session.h"
#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "audio/RtGuard.h"
#include "diag/CrashDump.h"
#include "diag/Diagnostics.h"
#include "diag/LogRing.h"
#include "plugins/Scanner.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <vector>

namespace ihub::app
{

//==============================================================================
// --rt-strict (T014)

namespace
{

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

} // namespace

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
int runListDevices()
{
    AudioEngine::DeviceManager deviceManager;
    std::cout << deviceManager.describeDevices() << std::endl;
    return 0;
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

    // The shipping set is deliberately silent until a patch is assigned. This
    // diagnostic needs a deterministic audible source, so it asks for the
    // engine's initialisation tone explicitly.
    config.slots = ihub::Session::makeDefaultSet();

    for (auto& slot : config.slots)
    {
        slot.kind = ihub::SessionSlotKind::initTone;
        slot.instrumentKind = "init-tone";
    }

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

} // namespace ihub::app
