#include "Scanner.h"

#include <algorithm>

namespace ihub::plugins
{
namespace
{
bool isVst3Bundle (const juce::File& file)
{
    return file.hasFileExtension ("vst3");
}

bool comparePaths (const juce::File& a, const juce::File& b)
{
    return a.getFullPathName() < b.getFullPathName();
}
} // namespace

Scanner::Scanner()
    : Scanner (Options{})
{
}

Scanner::Scanner (Options optionsToUse)
    : options (std::move (optionsToUse)),
      process ({ options.helperExecutable, options.timeoutMs, scanner_cli::defaultFormat })
{
    if (options.dataDirectory == juce::File())
        options.dataDirectory = defaultDataDirectory();

    reportFile = options.dataDirectory.getChildFile (reportFileName);

    juce::addHeadlessDefaultFormatsToManager (formatManager);

    for (auto* format : formatManager.getFormats())
        if (format->getName() == scanner_cli::defaultFormat)
            vst3Format = format;
}

juce::File Scanner::defaultDataDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Instrument Hub");
}

juce::File Scanner::defaultReportFile()
{
    return defaultDataDirectory().getChildFile (reportFileName);
}

void Scanner::log (const juce::String& message)
{
    if (options.logSink != nullptr)
    {
        options.logSink (message);
    }
    else
    {
        // Default JUCE logger: stderr for console builds, a FileLogger when the
        // application installed one.
        juce::Logger::writeToLog (message);
    }
}

bool Scanner::loadPersistedTypes()
{
    if (! options.loadExisting || ! reportFile.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse (reportFile);

    if (xml == nullptr)
    {
        log ("scanner: ignoring unreadable " + reportFile.getFullPathName());
        return false;
    }

    knownPlugins.recreateFromXml (*xml);

    return knownPlugins.getNumTypes() > 0;
}

juce::StringArray Scanner::discoverFiles (const juce::StringArray& directories)
{
    std::vector<juce::File> candidates;

    auto addCandidate = [&candidates] (const juce::File& file)
    {
        if (isVst3Bundle (file)
            && std::none_of (candidates.begin(), candidates.end(),
                             [&file] (const juce::File& other)
                             {
                                 return other.getFullPathName() == file.getFullPathName();
                             }))
        {
            candidates.push_back (file);
        }
    };

    auto scanOneDirectory = [this, &addCandidate] (const juce::File& directory)
    {
        for (const auto& path : vst3Format->searchPathsForPlugins (juce::FileSearchPath (directory.getFullPathName()),
                                                                   options.recursive,
                                                                   true))
            addCandidate (juce::File (path));
    };

    if (directories.isEmpty())
    {
        const auto defaults = vst3Format->getDefaultLocationsToSearch();

        for (int i = 0; i < defaults.getNumPaths(); ++i)
        {
            const auto directory = juce::File (defaults[i]);

            if (directory.isDirectory())
                scanOneDirectory (directory);
        }
    }
    else
    {
        for (const auto& path : directories)
        {
            const juce::File file (path);

            if (vst3Format->fileMightContainThisPluginType (file.getFullPathName()))
            {
                addCandidate (file);
            }
            else if (file.isDirectory())
            {
                scanOneDirectory (file);
            }
            else
            {
                log ("scanner: skipping missing path " + file.getFullPathName());
            }
        }
    }

    std::sort (candidates.begin(), candidates.end(), comparePaths);

    juce::StringArray result;

    for (const auto& candidate : candidates)
        result.add (candidate.getFullPathName());

    return result;
}

ScanReport Scanner::scan (const juce::FileSearchPath& directories)
{
    juce::StringArray paths;

    for (int i = 0; i < directories.getNumPaths(); ++i)
        paths.add (directories[i].getFullPathName());

    return scan (paths);
}

ScanReport Scanner::scan (const juce::StringArray& directories)
{
    ScanReport report;
    report.reportFile = reportFile;

    if (vst3Format == nullptr)
    {
        log ("scanner: no VST3 format available in this build");
        return report;
    }

    report.helperAvailable = process.getHelperExecutable().existsAsFile();

    if (! report.helperAvailable)
    {
        log ("scanner: failure: helper not found at "
             + process.getHelperExecutable().getFullPathName()
             + "; no plugin was scanned");
        return report;
    }

    if (! reportFile.getParentDirectory().createDirectory())
        log ("scanner: could not create " + reportFile.getParentDirectory().getFullPathName());

    if (loadPersistedTypes())
        log ("scanner: loaded " + juce::String (knownPlugins.getNumTypes())
             + " previously known plugin(s) from " + reportFile.getFullPathName());

    const auto files = discoverFiles (directories);
    report.filesDiscovered = files.size();

    for (const auto& path : files)
    {
        const auto result = process.scanFile (juce::File (path));

        if (result.succeeded)
        {
            ++report.succeeded;

            for (const auto& description : result.plugins)
            {
                knownPlugins.addType (description);
                report.plugins.push_back (description);
            }

            log ("scanner: ok: " + path + " (" + juce::String (result.plugins.size())
                 + " type(s), " + juce::String (result.elapsedMs, 1) + " ms)");
        }
        else
        {
            ++report.failed;

            ScanFailure failure;
            failure.file = juce::File (path);
            failure.reason = result.error;
            failure.crashed = result.crashed;
            failure.timedOut = result.timedOut;
            failure.helperOutput = result.output;
            report.failures.push_back (failure);

            log ("scanner: failure: " + path + " : " + result.error
                 + " (" + juce::String (result.elapsedMs, 1) + " ms)");
        }
    }

    if (const auto xml = knownPlugins.createXml())
        report.wroteReport = xml->writeTo (reportFile);
    else
        log ("scanner: could not serialise KnownPluginList");

    log ("scanner: done: " + juce::String (report.filesDiscovered) + " discovered, "
         + juce::String (report.succeeded) + " ok, " + juce::String (report.failed)
         + " failed, " + juce::String (knownPlugins.getNumTypes()) + " plugin(s) known"
         + (report.wroteReport ? "; wrote " + reportFile.getFullPathName()
                               : "; report was NOT written"));

    return report;
}
} // namespace ihub::plugins
