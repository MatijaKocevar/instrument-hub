#include "Scanner.h"

namespace ihub::plugins
{
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

    if (options.cacheFile == juce::File())
        options.cacheFile = options.dataDirectory.getChildFile (PluginCache::fileName);

    cache.setCacheFile (options.cacheFile);

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

} // namespace ihub::plugins
