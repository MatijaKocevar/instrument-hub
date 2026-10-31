#include "ScannerProcess.h"

#include <cstdlib>

namespace ihub::plugins
{
namespace
{
/** Removes the per-scan scratch directory however we leave scanFile(). */
struct ScopedTempDirectory
{
    explicit ScopedTempDirectory (const juce::File& directoryToOwn)
        : directory (directoryToOwn)
    {
    }

    ~ScopedTempDirectory()
    {
        directory.deleteRecursively();
    }

    juce::File directory;

    JUCE_DECLARE_NON_COPYABLE (ScopedTempDirectory)
};

juce::String abbreviate (const juce::String& text, int maxCharacters)
{
    auto trimmed = text.trim();

    if (trimmed.length() <= maxCharacters)
        return trimmed;

    return trimmed.substring (0, maxCharacters) + "...";
}

} // namespace

ScannerProcess::ScannerProcess (ScannerProcessOptions optionsToUse)
    : options (std::move (optionsToUse))
{
    if (options.helperExecutable == juce::File())
        options.helperExecutable = findHelperExecutable();

    if (options.timeoutMs <= 0)
        options.timeoutMs = 20000;

    if (options.formatName.isEmpty())
        options.formatName = scanner_cli::defaultFormat;
}

juce::File ScannerProcess::findHelperExecutable()
{
    if (auto* overridden = std::getenv ("IHUB_SCANNER"))
        if (*overridden != 0)
            return juce::File (juce::String::fromUTF8 (overridden)).getFullPathName();

   #if JUCE_WINDOWS
    const auto suffix = juce::String (".exe");
   #else
    const auto suffix = juce::String();
   #endif

    const auto appDirectory = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                  .getParentDirectory();

    return appDirectory.getChildFile (juce::String (scanner_cli::helperName) + suffix);
}

ScanResult ScannerProcess::scanFile (const juce::File& pluginBundle)
{
    ScanResult result;
    result.file = pluginBundle;

    const auto helper = options.helperExecutable;

    if (! helper.existsAsFile())
    {
        result.error = "scanner helper not found: " + helper.getFullPathName();
        return result;
    }

    // The helper writes its result here and nowhere else. If it dies before
    // finishing, the file is simply absent, which is how a crash is detected
    // (a signal-terminated child reports exit code 0 through JUCE).
    ScopedTempDirectory scratch (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("ihub-scanner-"
                                                    + juce::Uuid().toString()));

    if (! scratch.directory.createDirectory())
    {
        result.error = "could not create scanner scratch directory: "
                       + scratch.directory.getFullPathName();
        return result;
    }

    const auto resultFile = scratch.directory.getChildFile ("result.xml");

    juce::StringArray arguments;
    arguments.add (helper.getFullPathName());
    arguments.add (scanner_cli::scanFlag);
    arguments.add (pluginBundle.getFullPathName());
    arguments.add (scanner_cli::outFlag);
    arguments.add (resultFile.getFullPathName());
    arguments.add (scanner_cli::formatFlag);
    arguments.add (options.formatName);

    juce::ChildProcess child;
    const auto startTime = juce::Time::getMillisecondCounterHiRes();

    if (! child.start (arguments,
                       juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        result.error = "could not launch scanner helper: " + helper.getFullPathName();
        return result;
    }

    result.launched = true;

    const bool finished = child.waitForProcessToFinish (options.timeoutMs);

    if (! finished)
    {
        result.timedOut = true;
        child.kill();
        child.waitForProcessToFinish (2000);
    }

    result.elapsedMs = juce::Time::getMillisecondCounterHiRes() - startTime;
    result.output = child.readAllProcessOutput().trim();
    result.exitCode = static_cast<int> (child.getExitCode());

    std::unique_ptr<juce::XmlElement> xml;

    if (finished && resultFile.existsAsFile() && resultFile.getSize() > 0)
        xml = juce::XmlDocument::parse (resultFile);

    if (xml != nullptr)
    {
        // Parse the same document shape KnownPluginList::createXml() emits.
        // Doing it through PluginDescription::loadFromXml (headless) keeps the
        // helper and this wrapper independent of the GUI host module.
        if (xml->hasTagName ("KNOWNPLUGINS"))
        {
            for (auto* childElement : xml->getChildIterator())
            {
                juce::PluginDescription description;

                if (childElement->hasTagName ("BLACKLISTED"))
                    continue;

                if (description.loadFromXml (*childElement))
                    result.plugins.push_back (description);
            }
        }
    }

    if (result.timedOut)
    {
        result.error = "timed out after " + juce::String (options.timeoutMs) + " ms";
    }
    else if (xml == nullptr)
    {
        // Covers both "helper crashed" (signal, no file) and "helper wrote
        // garbage". Either way no usable scan result exists. JUCE reports a
        // signal-terminated child as exit code 0, hence the explicit wording.
        result.crashed = true;

        if (result.exitCode != 0)
            result.error = "helper exited with code " + juce::String (result.exitCode)
                           + " before writing a result";
        else
            result.error = "helper crashed while loading the plugin"
                           " (no result file; exit code 0)";
    }
    else if (result.exitCode != 0)
    {
        result.error = "helper exited with code " + juce::String (result.exitCode);
    }
    else if (result.plugins.empty())
    {
        result.error = "no plugin types found in bundle";
    }
    else
    {
        result.succeeded = true;
    }

    if (! result.succeeded && result.output.isNotEmpty())
        result.error << " [" << abbreviate (result.output, 400) << "]";

    return result;
}
} // namespace ihub::plugins
