#include "Diagnostics.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <sstream>

namespace ihub::diag
{

namespace
{
std::string text (const juce::String& value)
{
    return value.toStdString();
}

std::string number (double value, int decimals)
{
    return text (juce::String (value, decimals));
}

std::string bufferMilliseconds (double sampleRate, int bufferSize)
{
    if (sampleRate <= 0.0 || bufferSize <= 0)
        return "0.000";

    return number (static_cast<double> (bufferSize) * 1000.0 / sampleRate, 3);
}

std::string latencyMilliseconds (int samples, double sampleRate)
{
    if (sampleRate <= 0.0 || samples < 0)
        return "0.000";

    return number (static_cast<double> (samples) * 1000.0 / sampleRate, 3);
}

std::string machineTime (std::int64_t millisecondsSinceEpoch)
{
    return text (juce::Time (millisecondsSinceEpoch).toISO8601 (true));
}

bool samePlugin (const PluginDiagnostics& a, const PluginDiagnostics& b)
{
    return a.name == b.name && a.file == b.file && a.format == b.format;
}
}

std::vector<PluginDiagnostics> loadKnownPlugins (const juce::File& file)
{
    std::vector<PluginDiagnostics> result;

    if (! file.existsAsFile())
        return result;

    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("KNOWNPLUGINS"))
        return result;

    for (const auto* element : xml->getChildWithTagNameIterator ("PLUGIN"))
    {
        juce::PluginDescription description;

        if (! description.loadFromXml (*element))
            continue;

        PluginDiagnostics plugin;
        plugin.name = text (description.name);
        plugin.manufacturer = text (description.manufacturerName);
        plugin.version = text (description.version);
        plugin.format = text (description.pluginFormatName);
        plugin.file = text (description.fileOrIdentifier);
        plugin.isInstrument = description.isInstrument;
        plugin.loaded = false;
        result.push_back (std::move (plugin));
    }

    return result;
}

DiagnosticsSnapshot collectDiagnostics (const AudioEngine::DeviceManager& device,
                                        const AudioEngine::TimingSnapshot& timing,
                                        const std::vector<PluginDiagnostics>& loadedPlugins,
                                        const LogRing& log,
                                        const DiagnosticsOptions& options)
{
    DiagnosticsSnapshot snapshot;

    snapshot.generatedAt = text (juce::Time::getCurrentTime().toISO8601 (true));

    snapshot.build.applicationName = "Instrument Hub";

   #if defined (JUCE_APPLICATION_VERSION_STRING)
    snapshot.build.version = JUCE_APPLICATION_VERSION_STRING;
   #else
    snapshot.build.version = "0.0.0";
   #endif

   #if JUCE_DEBUG
    snapshot.build.buildType = "Debug";
   #else
    snapshot.build.buildType = "Release";
   #endif

    snapshot.build.juceVersion = text (juce::String (JUCE_MAJOR_VERSION) + "."
                                       + juce::String (JUCE_MINOR_VERSION) + "."
                                       + juce::String (JUCE_BUILDNUMBER));
    snapshot.build.operatingSystem = text (juce::SystemStats::getOperatingSystemName());
    snapshot.build.cpuCores = juce::SystemStats::getNumCpus();

    snapshot.device.open = device.isOpen();
    snapshot.device.type = text (device.getCurrentDeviceTypeName());
    snapshot.device.name = text (device.getCurrentDeviceName());
    snapshot.device.sampleRate = device.getCurrentSampleRate();
    snapshot.device.bufferSize = device.getCurrentBufferSize();
    snapshot.device.bitDepth = device.getCurrentBitDepth();
    snapshot.device.inputLatencySamples = device.getInputLatencyInSamples();
    snapshot.device.outputLatencySamples = device.getOutputLatencyInSamples();
    snapshot.device.deviceXruns = static_cast<std::uint64_t> (
        std::max (0, device.getXRunCount()));

    if (snapshot.device.open)
    {
        const auto setup = device.getCurrentSetup();
        snapshot.device.inputChannels = setup.numInputChannels;
        snapshot.device.outputChannels = setup.numOutputChannels;
    }

    snapshot.timing = timing;

    snapshot.plugins = loadedPlugins;

    snapshot.knownPluginListFile = options.knownPluginListFile;
    snapshot.knownPluginListPresent = options.knownPluginListFile.existsAsFile();

    for (auto& known : loadKnownPlugins (options.knownPluginListFile))
    {
        const bool exists = std::any_of (snapshot.plugins.begin(),
                                         snapshot.plugins.end(),
                                         [&known] (const PluginDiagnostics& plugin)
                                         {
                                             return samePlugin (plugin, known);
                                         });

        if (! exists)
            snapshot.plugins.push_back (std::move (known));
    }

    const auto maxRecords = static_cast<std::size_t> (
        juce::jlimit (0, 4096, options.maxLogRecords));

    snapshot.logPushed = log.getPushedCount();
    snapshot.logOverwritten = log.getOverwrittenCount();

    std::vector<LogRecord> records (maxRecords);

    if (maxRecords > 0)
    {
        const auto copied = log.copyLatest (records.data(), records.size());
        records.resize (copied);
    }

    snapshot.recentLog = std::move (records);
    snapshot.crashDumpPath = options.crashDumpPath;

    return snapshot;
}

std::string formatDiagnostics (const DiagnosticsSnapshot& snapshot)
{
    std::ostringstream out;

    const auto& build = snapshot.build;
    const auto& audio = snapshot.device;
    const auto& timing = snapshot.timing;

    out << "Instrument Hub diagnostics\n";
    out << "generated: " << snapshot.generatedAt << "\n";
    out << "build: " << build.buildType << ", version " << build.version
        << ", JUCE " << build.juceVersion << "\n";
    out << "system: " << build.operatingSystem << ", " << build.cpuCores
        << " CPUs\n";
    out << "crash dump: " << (snapshot.crashDumpPath.empty()
                                  ? std::string ("(disabled)")
                                  : snapshot.crashDumpPath)
        << "\n";

    out << "\n[audio]\n";
    out << "status: " << (audio.open ? "open" : "closed") << "\n";
    out << "device: " << (audio.name.empty() ? "(none)" : audio.name) << "\n";
    out << "device type: " << (audio.type.empty() ? "(none)" : audio.type) << "\n";
    out << "sample rate: " << number (audio.sampleRate, 0) << " Hz\n";
    out << "buffer: " << audio.bufferSize << " frames ("
        << bufferMilliseconds (audio.sampleRate, audio.bufferSize) << " ms)\n";
    out << "bit depth: " << audio.bitDepth << "\n";
    out << "channels: " << audio.outputChannels << " out, "
        << audio.inputChannels << " in\n";
    out << "latency: input " << audio.inputLatencySamples << " samples ("
        << latencyMilliseconds (audio.inputLatencySamples, audio.sampleRate)
        << " ms), output " << audio.outputLatencySamples << " samples ("
        << latencyMilliseconds (audio.outputLatencySamples, audio.sampleRate)
        << " ms), buffer "
        << bufferMilliseconds (audio.sampleRate, audio.bufferSize) << " ms, total "
        << number (static_cast<double> (audio.bufferSize
                                        + audio.inputLatencySamples
                                        + audio.outputLatencySamples)
                       * 1000.0 / (audio.sampleRate > 0.0 ? audio.sampleRate : 1.0),
                   3)
        << " ms\n";
    out << "xruns: device " << audio.deviceXruns
        << ", callback " << timing.overruns
        << ", dropped blocks " << timing.droppedBlocks << "\n";

    out << "\n[callbacks]\n";
    out << "callbacks: " << timing.callbacks << "\n";
    out << "samples processed: " << timing.samplesProcessed << "\n";
    out << "deadline: " << number (timing.deadlineMs, 3) << " ms\n";
    out << "last: " << number (timing.lastCallbackMs, 3) << " ms ("
        << number (timing.lastLoadPercent, 1) << "% load)\n";
    out << "average: " << number (timing.averageCallbackMs, 3) << " ms\n";
    out << "rolling max: " << number (timing.rollingMaxCallbackMs, 3)
        << " ms over " << timing.windowSize << " callbacks\n";
    out << "worst: " << number (timing.worstCallbackMs, 3) << " ms\n";

    out << "\n[plugins]\n";
    const auto loaded = static_cast<int> (std::count_if (
        snapshot.plugins.begin(), snapshot.plugins.end(),
        [] (const PluginDiagnostics& plugin) { return plugin.loaded; }));

    out << "loaded: " << loaded << ", known: "
        << static_cast<int> (snapshot.plugins.size()) - loaded << "\n";
    out << "plugin list: " << text (snapshot.knownPluginListFile.getFullPathName())
        << (snapshot.knownPluginListPresent ? "" : " (missing)") << "\n";

    if (snapshot.plugins.empty())
    {
        out << "  (no plugins)\n";
    }
    else
    {
        for (const auto& plugin : snapshot.plugins)
        {
            out << "  - " << (plugin.name.empty() ? "(unnamed)" : plugin.name);

            if (! plugin.version.empty())
                out << " " << plugin.version;

            out << " [" << (plugin.format.empty() ? "unknown" : plugin.format) << "]";

            if (! plugin.manufacturer.empty())
                out << " by " << plugin.manufacturer;

            out << (plugin.isInstrument ? " instrument" : " effect");

            if (plugin.loaded)
                out << ", loaded in slot " << plugin.slot;
            else
                out << ", known";

            if (! plugin.file.empty())
                out << ", " << plugin.file;

            out << "\n";
        }
    }

    out << "\n[log]\n";
    out << "records: " << snapshot.recentLog.size() << " of "
        << snapshot.logPushed << " (" << snapshot.logOverwritten
        << " overwritten)\n";

    for (const auto& record : snapshot.recentLog)
    {
        out << "  " << machineTime (record.timestampMs)
            << " " << toString (record.level)
            << " " << toString (record.source)
            << " [" << record.code << "] "
            << record.message << "\n";
    }

    return out.str();
}

bool writeDiagnostics (const DiagnosticsSnapshot& snapshot, const juce::File& file)
{
    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::String (formatDiagnostics (snapshot)));
}

}
