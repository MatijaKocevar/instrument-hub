// ihub-diagnostics-test: acceptance support for T111.
//
// 1. Structured log ring: field round-trip, truncation, overwrite accounting,
//    newest-first selection when the destination is smaller than the ring, and
//    a concurrent four-producer/one-reader stress test that proves no torn
//    record is ever returned.
// 2. Crash-safe dump: the exact `writeCrashDump()` byte format, then a forked
//    child that raises SIGABRT with the handler installed. The parent requires
//    the original signal to survive and the dump file to contain the records.
// 3. Diagnostics: parse a KnownPluginList.xml, merge loaded and known plugins,
//    and assert the formatted report names the device, buffer, latency, xruns
//    and plugins.

#include "audio/TimingMonitor.h"
#include "diag/CrashDump.h"
#include "diag/Diagnostics.h"
#include "diag/LogRing.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if ! defined(_WIN32)
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool contains (const std::string& text, const std::string& needle)
{
    return text.find (needle) != std::string::npos;
}

juce::File tempFile (const juce::String& suffix)
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("ihub-diagnostics", suffix, false);
}

//==============================================================================
void testStructuredRecords()
{
    std::cout << "\n-- structured records --" << std::endl;

    ihub::diag::LogRing ring (4);

    ring.log (ihub::diag::LogLevel::warning,
              ihub::diag::LogSource::audio,
              42,
              "buffer underrun");

    ihub::diag::LogRecord records[4] {};
    auto count = ring.copyLatest (records, 4);

    check (count == 1, "one record is readable");
    check (records[0].sequence == 0, "the ring assigns sequence numbers from 0");
    check (records[0].level == ihub::diag::LogLevel::warning, "the level is stored");
    check (records[0].source == ihub::diag::LogSource::audio, "the source is stored");
    check (records[0].code == 42, "the structured code is stored");
    check (std::string (records[0].message) == "buffer underrun", "the message is stored");
    check (records[0].timestampMs > 0, "the record has a wall-clock timestamp");

    for (int index = 0; index < 12; ++index)
        ring.log (ihub::diag::LogLevel::info,
                  ihub::diag::LogSource::general,
                  index,
                  "record " + std::to_string (index));

    count = ring.copyLatest (records, 4);

    check (count == 4, "a full ring returns its capacity");
    check (records[0].code == 8 && records[3].code == 11,
           "the newest four records are returned, oldest first");
    check (records[0].sequence == 9 && records[3].sequence == 12,
           "returned records keep their monotonic sequence numbers");
    check (ring.getPushedCount() == 13, "every record is counted");
    check (ring.getOverwrittenCount() == 9,
           "records dropped by the wrap are reported as overwritten");

    // A destination smaller than the ring must get the newest records, not
    // the oldest ones.
    ihub::diag::LogRecord two[2] {};
    check (ring.copyLatest (two, 2) == 2 && two[0].code == 10 && two[1].code == 11,
           "a small destination receives the newest records");

    // Long messages truncate and stay terminated.
    ring.log (ihub::diag::LogLevel::error,
              ihub::diag::LogSource::diagnostics,
              7,
              std::string (ihub::diag::logMessageCapacity * 4, 'x'));

    ihub::diag::LogRecord longest {};
    ring.copyLatest (&longest, 1);

    check (longest.message[ihub::diag::logMessageCapacity - 1] == '\0',
           "a long message is terminated inside the record");
    check (std::strlen (longest.message) == ihub::diag::logMessageCapacity - 1,
           "a long message is truncated to the record capacity");
}

void testLevelAndSourceNames()
{
    std::cout << "\n-- level and source parsing --" << std::endl;

    ihub::diag::LogLevel level = ihub::diag::LogLevel::info;
    ihub::diag::LogSource source = ihub::diag::LogSource::general;

    check (ihub::diag::parseLogLevel ("warning", level)
               && level == ihub::diag::LogLevel::warning,
           "warning parses case-insensitively");
    check (ihub::diag::parseLogSource ("PLUGINS", source)
               && source == ihub::diag::LogSource::plugins,
           "source names parse case-insensitively");
    check (! ihub::diag::parseLogLevel ("nonsense", level),
           "an unknown level is rejected");
    check (std::string (ihub::diag::toString (ihub::diag::LogLevel::fatal)) == "FATAL",
           "levels have stable text");
    check (std::string (ihub::diag::toString (ihub::diag::LogSource::audio)) == "audio",
           "sources have stable text");
}

void testConcurrentWriters()
{
    std::cout << "\n-- concurrent producers --" << std::endl;

    constexpr int producerCount = 4;
    constexpr int recordsPerProducer = 500;
    constexpr int capacity = 256;

    ihub::diag::LogRing ring (capacity);

    std::atomic<bool> start { false };
    std::atomic<bool> running { true };
    std::atomic<int> torn { 0 };
    std::atomic<int> read { 0 };

    std::vector<std::thread> producers;

    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back ([&ring, &start, producer]
        {
            while (! start.load())
                std::this_thread::yield();

            for (int index = 0; index < recordsPerProducer; ++index)
                ring.log (ihub::diag::LogLevel::info,
                          ihub::diag::LogSource::general,
                          producer,
                          "producer " + std::to_string (producer)
                              + " record " + std::to_string (index));
        });
    }

    std::thread reader ([&ring, &running, &torn, &read]
    {
        ihub::diag::LogRecord buffer[capacity];

        while (running.load())
        {
            const auto count = ring.copyLatest (buffer, capacity);

            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& record = buffer[index];

                if (std::memchr (record.message, '\0',
                                 ihub::diag::logMessageCapacity) == nullptr)
                {
                    ++torn;
                    continue;
                }

                const auto expected = "producer " + std::to_string (record.code)
                                      + " record ";

                if (std::string (record.message).rfind (expected, 0) != 0)
                    ++torn;

                if (record.level != ihub::diag::LogLevel::info
                    || record.source != ihub::diag::LogSource::general)
                {
                    ++torn;
                }

                ++read;
            }

            std::this_thread::yield();
        }
    });

    start.store (true);

    for (auto& producer : producers)
        producer.join();

    running.store (false);
    reader.join();

    check (torn.load() == 0, "no reader observed a torn record");
    check (read.load() > 0, "the reader observed records while writers ran");
    check (ring.getPushedCount() == producerCount * recordsPerProducer,
           "all producers' records were accepted");

    ihub::diag::LogRecord newest[capacity];
    const auto count = ring.copyLatest (newest, capacity);
    bool contiguous = count == capacity;

    for (std::size_t index = 0; index < count && contiguous; ++index)
        contiguous = newest[index].sequence
                     == ring.getPushedCount() - count + index;

    check (contiguous, "the final window holds contiguous, newest records");
}

//==============================================================================
void testCrashDumpFormat()
{
#if ! defined(_WIN32)
    std::cout << "\n-- crash dump format --" << std::endl;

    const auto file = tempFile (".log");
    const auto path = file.getFullPathName().toStdString();

    ihub::diag::LogRing ring (8);
    ring.log (ihub::diag::LogLevel::info,
              ihub::diag::LogSource::audio,
              1,
              "device opened");
    ring.log (ihub::diag::LogLevel::fatal,
              ihub::diag::LogSource::general,
              99,
              "about to fall over");

    const int fd = ::open (path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    check (fd >= 0, "the dump file opens");

    const bool written = fd >= 0
                         && ihub::diag::writeCrashDump (fd, ring, SIGABRT);
    if (fd >= 0)
        ::close (fd);

    check (written, "writeCrashDump reports success");

    const auto content = file.loadFileAsString().toStdString();

    check (contains (content, "instrument-hub crash dump v1"),
           "the dump starts with the version header");
    check (contains (content, "signal: 6 SIGABRT"), "the signal is recorded");
    check (contains (content, "records: 2"), "the record count is recorded");
    check (contains (content, "device opened"), "the first record is present");
    check (contains (content, "about to fall over"), "the last record is present");
    check (contains (content, "FATAL general 99 about to fall over"),
           "records carry level, source and structured code");
    check (contains (content, "\nend\n"), "the dump has a terminator");

    file.deleteFile();
#endif
}

void testSignalHandler()
{
#if ! defined(_WIN32)
    std::cout << "\n-- crash handler --" << std::endl;

    const auto file = tempFile (".log");
    const auto path = file.getFullPathName().toStdString();
    file.deleteFile();

    const pid_t pid = ::fork();

    if (pid == 0)
    {
        // Child: install on a local ring, leave a trail, then really crash.
        ihub::diag::LogRing ring (8);
        ring.log (ihub::diag::LogLevel::info,
                  ihub::diag::LogSource::diagnostics,
                  1,
                  "before the crash");

        ihub::diag::CrashDumpOptions options;
        options.path = path;
        options.ring = &ring;

        if (! ihub::diag::installCrashDump (options))
            ::_exit (97);

        ring.log (ihub::diag::LogLevel::info,
                  ihub::diag::LogSource::diagnostics,
                  2,
                  "handler installed");

        ::raise (SIGABRT);
        ::_exit (98);
    }

    int status = 0;
    const auto waited = ::waitpid (pid, &status, 0);

    check (waited == pid, "the child was reaped");

    const bool signalled = WIFSIGNALED (status);
    check (signalled && WTERMSIG (status) == SIGABRT,
           "the original SIGABRT status is preserved");

    const auto content = file.loadFileAsString().toStdString();

    check (contains (content, "SIGABRT"), "the handler recorded SIGABRT");
    check (contains (content, "before the crash"),
           "records written before the handler was installed are dumped");
    check (contains (content, "handler installed"),
           "records written after installation are dumped");

    file.deleteFile();
#endif
}

//==============================================================================
void testKnownPluginsAndMerge()
{
    std::cout << "\n-- plugin diagnostics --" << std::endl;

    const auto xmlFile = tempFile (".xml");

    xmlFile.replaceWithText (
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<KNOWNPLUGINS>\n"
        "  <PLUGIN name=\"Diagnostics Test Plugin\" format=\"VST3\"\n"
        "          category=\"Instrument|Synth\" manufacturer=\"Instrument Hub\"\n"
        "          version=\"1.2.3\" file=\"/tmp/TestPlugin.vst3\"\n"
        "          uniqueId=\"12345678\" isInstrument=\"1\" fileTime=\"0\"\n"
        "          infoUpdateTime=\"0\" numInputs=\"2\" numOutputs=\"2\"\n"
        "          isShell=\"0\" hasARAExtension=\"0\" uid=\"deadbeef\"/>\n"
        "</KNOWNPLUGINS>\n");

    const auto known = ihub::diag::loadKnownPlugins (xmlFile);

    check (known.size() == 1, "one known plugin is parsed");
    check (! known.empty() && known[0].name == "Diagnostics Test Plugin",
           "the plugin name is parsed");
    check (! known.empty() && known[0].format == "VST3", "the format is parsed");
    check (! known.empty() && known[0].isInstrument, "the instrument flag is parsed");

    ihub::diag::LogRing log (16);
    log.log (ihub::diag::LogLevel::info, ihub::diag::LogSource::audio, 1, "one");
    log.log (ihub::diag::LogLevel::info, ihub::diag::LogSource::audio, 2, "two");
    log.log (ihub::diag::LogLevel::info, ihub::diag::LogSource::audio, 3, "three");

    ihub::diag::PluginDiagnostics loadedPlugin;
    loadedPlugin.name = "Loaded Plugin";
    loadedPlugin.format = "VST3";
    loadedPlugin.loaded = true;
    loadedPlugin.slot = 2;

    AudioEngine::DeviceManager device;
    AudioEngine::TimingSnapshot timing;

    ihub::diag::DiagnosticsOptions options;
    options.knownPluginListFile = xmlFile;
    options.crashDumpPath = "/tmp/instrument-hub-crash.log";
    options.maxLogRecords = 2;

    const auto snapshot = ihub::diag::collectDiagnostics (
        device, timing, { loadedPlugin }, log, options);

    check (snapshot.plugins.size() == 2, "loaded and known plugins are merged");
    check (! snapshot.plugins.empty() && snapshot.plugins[0].loaded,
           "the loaded instance comes first");
    check (snapshot.plugins[0].slot == 2, "the loaded slot is recorded");
    check (snapshot.recentLog.size() == 2, "the log limit is honoured");
    check (snapshot.recentLog.size() == 2
               && std::string (snapshot.recentLog[1].message) == "three",
           "the newest log records are kept");
    check (snapshot.logPushed == 3 && snapshot.logOverwritten == 0,
           "log counters are reported");
    check (snapshot.knownPluginListPresent, "the plugin list is marked present");

    xmlFile.deleteFile();
}

void testFormattedReport()
{
    std::cout << "\n-- formatted report --" << std::endl;

    ihub::diag::DiagnosticsSnapshot snapshot;
    snapshot.generatedAt = "2026-10-31T12:00:00";
    snapshot.build.applicationName = "Instrument Hub";
    snapshot.build.version = "0.1.0";
    snapshot.build.buildType = "RelWithDebInfo";
    snapshot.build.juceVersion = "8.0.0";
    snapshot.build.operatingSystem = "Linux";
    snapshot.build.cpuCores = 8;

    snapshot.device.open = true;
    snapshot.device.type = "ALSA";
    snapshot.device.name = "Loopback";
    snapshot.device.sampleRate = 48000.0;
    snapshot.device.bufferSize = 128;
    snapshot.device.bitDepth = 32;
    snapshot.device.inputChannels = 2;
    snapshot.device.outputChannels = 2;
    snapshot.device.inputLatencySamples = 0;
    snapshot.device.outputLatencySamples = 0;
    snapshot.device.deviceXruns = 2;

    snapshot.timing.callbacks = 375;
    snapshot.timing.overruns = 1;
    snapshot.timing.droppedBlocks = 0;
    snapshot.timing.samplesProcessed = 48000;
    snapshot.timing.sampleRate = 48000.0;
    snapshot.timing.blockSize = 128;
    snapshot.timing.deadlineMs = 128.0 * 1000.0 / 48000.0;
    snapshot.timing.lastCallbackMs = 0.5;
    snapshot.timing.rollingMaxCallbackMs = 1.25;
    snapshot.timing.worstCallbackMs = 1.25;
    snapshot.timing.averageCallbackMs = 0.4;
    snapshot.timing.lastLoadPercent = 18.75;
    snapshot.timing.windowSize = 512;

    ihub::diag::PluginDiagnostics loaded;
    loaded.name = "Analog Lab V";
    loaded.manufacturer = "Arturia";
    loaded.version = "5.12";
    loaded.format = "VST3";
    loaded.isInstrument = true;
    loaded.loaded = true;
    loaded.slot = 0;

    ihub::diag::PluginDiagnostics known;
    known.name = "TestPlugin";
    known.format = "VST3";
    known.loaded = false;

    snapshot.plugins = { loaded, known };
    snapshot.knownPluginListFile = juce::File ("/tmp/KnownPluginList.xml");
    snapshot.knownPluginListPresent = true;
    snapshot.crashDumpPath = "/tmp/instrument-hub-crash.log";

    ihub::diag::LogRing log (8);
    log.log (ihub::diag::LogLevel::info,
             ihub::diag::LogSource::diagnostics,
             7,
             "report generated");

    ihub::diag::LogRecord record {};
    log.copyLatest (&record, 1);
    snapshot.recentLog = { record };
    snapshot.logPushed = 1;

    const auto report = ihub::diag::formatDiagnostics (snapshot);

    check (contains (report, "Instrument Hub diagnostics"), "the report has a title");
    check (contains (report, "device: Loopback"), "the device name is printed");
    check (contains (report, "device type: ALSA"), "the device type is printed");
    check (contains (report, "buffer: 128 frames (2.667 ms)"),
           "the buffer size and duration are printed");
    check (contains (report, "latency: input 0 samples"), "input latency is printed");
    check (contains (report, "total 2.667 ms"), "the total latency is printed");
    check (contains (report, "xruns: device 2, callback 1, dropped blocks 0"),
           "device and callback xruns are printed");
    check (contains (report, "callbacks: 375"), "the callback count is printed");
    check (contains (report, "loaded: 1, known: 1"),
           "loaded and known plugin counts are printed");
    check (contains (report, "Analog Lab V") && contains (report, "loaded in slot 0"),
           "the loaded plugin is printed with its slot");
    check (contains (report, "TestPlugin") && contains (report, "known"),
           "the known plugin is printed");
    check (contains (report, "records: 1 of 1"), "the log summary is printed");
    check (contains (report, "report generated"), "the newest log line is printed");
    check (contains (report, "crash dump: /tmp/instrument-hub-crash.log"),
           "the crash dump path is printed");
}

} // namespace

int main()
{
    std::cout << "ihub-diagnostics-test" << std::endl;

    testStructuredRecords();
    testLevelAndSourceNames();
    testConcurrentWriters();
    testCrashDumpFormat();
    testSignalHandler();
    testKnownPluginsAndMerge();
    testFormattedReport();

    std::cout << "\n" << (failures == 0 ? "PASS" : "FAIL")
              << ": " << failures << " failure(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
