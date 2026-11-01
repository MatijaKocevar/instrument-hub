#include "plugins/Scanner.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool anyLineContains (const std::vector<juce::String>& lines,
                      const juce::String& first,
                      const juce::String& second)
{
    return std::any_of (lines.begin(), lines.end(), [&] (const juce::String& line)
    {
        return line.contains (first) && line.contains (second);
    });
}

bool hasTypeNamed (const juce::KnownPluginList& list, const juce::String& name)
{
    for (const auto& type : list.getTypes())
        if (type.name.contains (name))
            return true;

    return false;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File goodBundle = argc > 1 ? juce::File (juce::String (argv[1]))
                                           : juce::File (IHUB_TESTPLUGIN_VST3);
    const juce::File crashingBundle = argc > 2 ? juce::File (juce::String (argv[2]))
                                               : juce::File (IHUB_CRASHING_VST3);
    const juce::File helper = argc > 3 ? juce::File (juce::String (argv[3]))
                                       : juce::File (IHUB_SCANNER_HELPER);

    std::cout << "test plugin: " << goodBundle.getFullPathName() << std::endl;
    std::cout << "crashing fake: " << crashingBundle.getFullPathName() << std::endl;
    std::cout << "helper: " << helper.getFullPathName() << std::endl;

    check (goodBundle.isDirectory(), "TestPlugin.vst3 bundle exists");
    check (crashingBundle.isDirectory(), "CrashingPlugin.vst3 fake bundle exists");
    check (helper.existsAsFile(), "ihub-scanner helper exists");

    if (failures != 0)
        return 1;

    const auto workspace = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-scanner-acceptance-"
                                              + juce::Uuid().toString());

    workspace.deleteRecursively();
    workspace.createDirectory();

    const auto scanDirectory = workspace.getChildFile ("vst3");
    const auto dataDirectory = workspace.getChildFile ("user-data");
    scanDirectory.createDirectory();
    dataDirectory.createDirectory();

    check (goodBundle.copyDirectoryTo (scanDirectory.getChildFile ("TestPlugin.vst3")),
           "copied TestPlugin.vst3 into the scan directory");
    check (crashingBundle.copyDirectoryTo (scanDirectory.getChildFile ("CrashingPlugin.vst3")),
           "copied CrashingPlugin.vst3 into the scan directory");

    check (! scanDirectory.getChildFile ("CrashingPlugin.vst3")
                .getChildFile ("Contents/Resources/moduleinfo.json")
                .existsAsFile(),
           "the crashing fake has no moduleinfo.json fast path");

    std::vector<juce::String> logLines;

    ihub::plugins::Scanner::Options options;
    options.dataDirectory = dataDirectory;
    options.helperExecutable = helper;
    options.timeoutMs = 20000;
    options.loadExisting = false;
    options.logSink = [&logLines] (const juce::String& line)
    {
        logLines.push_back (line);
        std::cout << "log: " << line << std::endl;
    };

    ihub::plugins::Scanner scanner (options);

    juce::StringArray directories;
    directories.add (scanDirectory.getFullPathName());

    const auto report = scanner.scan (directories);

    check (report.filesDiscovered == 2, "both bundles were discovered");
    check (report.failed == 1, "exactly one plugin failed to scan");
    check (report.succeeded == 1, "exactly one plugin scanned successfully");

    check (report.failures.size() == 1, "one failure was recorded");

    if (report.failures.size() == 1)
    {
        const auto& failure = report.failures[0];

        std::cout << "failure: " << failure.file.getFullPathName() << std::endl;
        std::cout << "reason: " << failure.reason << std::endl;

        check (failure.file.getFileName().contains ("CrashingPlugin"),
               "the failure names CrashingPlugin.vst3");
        check (failure.crashed, "the failure is classified as a helper crash");
        check (failure.reason.contains ("crashed"),
               "the failure reason says the helper crashed");
    }

    check (anyLineContains (logLines, "failure", "CrashingPlugin"),
           "a 'failure' line was logged for the crashing plugin");

    std::cout << "survived: the scanner returned after the crashing fake was loaded"
              << std::endl;

    const auto goodBundlePath = scanDirectory.getChildFile ("TestPlugin.vst3").getFullPathName();
    const auto knownGood = scanner.getKnownPlugins().getTypeForFile (goodBundlePath);

    check (hasTypeNamed (scanner.getKnownPlugins(), "TestPlugin"),
           "TestPlugin is still in the in-memory KnownPluginList");

    check (knownGood != nullptr, "the listed type is keyed by the scanned bundle path");

    if (knownGood != nullptr)
        check (knownGood->fileOrIdentifier == goodBundlePath,
               "the listed TestPlugin points at the scanned bundle");

    check (scanner.getKnownPlugins().getNumTypes() == 1,
           "the crashing plugin contributed no types");

    const auto xmlFile = dataDirectory.getChildFile ("KnownPluginList.xml");

    check (report.wroteReport, "the scan reports that the XML was written");
    check (report.reportFile == xmlFile, "the report path is the user data dir file");
    check (xmlFile.existsAsFile(), "KnownPluginList.xml exists in the user data dir");

    if (const auto xml = juce::XmlDocument::parse (xmlFile))
    {
        juce::KnownPluginList persisted;
        persisted.recreateFromXml (*xml);

        std::cout << "persisted: " << persisted.getNumTypes() << " type(s) in "
                  << xmlFile.getFullPathName() << std::endl;

        for (const auto& type : persisted.getTypes())
            std::cout << "persisted type: " << type.name << " -> "
                      << type.fileOrIdentifier << std::endl;

        check (persisted.getNumTypes() == 1, "the XML holds exactly one plugin");
        check (hasTypeNamed (persisted, "TestPlugin"),
               "the XML still lists TestPlugin after the crash");
        check (! hasTypeNamed (persisted, "CrashingPlugin"),
               "the XML does not list the crashing plugin");
    }
    else
    {
        check (false, "the written KnownPluginList.xml is parseable");
    }

    const auto secondReport = scanner.scan (directories);

    check (secondReport.failed == 1 && secondReport.succeeded == 1,
           "a second scan also reports one ok and one crashed plugin");
    check (hasTypeNamed (scanner.getKnownPlugins(), "TestPlugin"),
           "TestPlugin is still listed after the second crash");

    workspace.deleteRecursively();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
