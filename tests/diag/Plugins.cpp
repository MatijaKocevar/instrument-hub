#include "Plugins.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"
#include "diag/Diagnostics.h"
#include "diag/LogRing.h"

#include <iostream>
#include <string>

namespace DiagTest
{

void runPluginMerge()
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

} // namespace DiagTest
