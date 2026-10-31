// ihub-scanner: out-of-process VST3 scan helper (T021).
//
// This binary loads exactly one plugin bundle, turns the result into a
// KnownPluginList XML document and exits. It deliberately does no other work,
// so a plugin that crashes during load can only end this process. The parent
// (plugins::ScannerProcess) treats a missing result file as a crash and keeps
// the rest of the scan running.
//
// Usage:
//   ihub-scanner --scan <bundle.vst3> --out <result.xml> [--format VST3]
//
// Exit codes: 0 scanned, 2 bad arguments, 3 format unavailable or result not
// written. A plugin crash therefore shows up as a signal, not an exit code.

#include "ScannerProcess.h"
#include "ProcessLimits.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <iostream>

#if defined(RLIMIT_CORE)
 #include <sys/resource.h>
#endif

namespace
{
struct Arguments
{
    juce::File scanFile;
    juce::File outFile;
    juce::String format { ihub::plugins::scanner_cli::defaultFormat };
    bool valid = false;
    juce::String error;
};

Arguments parseArguments (const juce::StringArray& arguments)
{
    Arguments parsed;

    for (int i = 1; i < arguments.size(); ++i)
    {
        const auto& argument = arguments[i];

        auto takeValue = [&arguments, &i, &parsed] (juce::File& target) -> bool
        {
            if (i + 1 >= arguments.size())
            {
                parsed.error = "missing value after " + arguments[i];
                return false;
            }

            target = juce::File (juce::String (arguments[++i])).getFullPathName();
            return true;
        };

        if (argument == ihub::plugins::scanner_cli::scanFlag)
        {
            if (! takeValue (parsed.scanFile))
                return parsed;
        }
        else if (argument == ihub::plugins::scanner_cli::outFlag)
        {
            if (! takeValue (parsed.outFile))
                return parsed;
        }
        else if (argument == ihub::plugins::scanner_cli::formatFlag)
        {
            if (i + 1 >= arguments.size())
            {
                parsed.error = "missing value after " + argument;
                return parsed;
            }

            parsed.format = arguments[++i];
        }
        else
        {
            parsed.error = "unknown argument: " + argument;
            return parsed;
        }
    }

    if (parsed.scanFile == juce::File())
        parsed.error = "no --scan path given";
    else if (parsed.outFile == juce::File())
        parsed.error = "no --out path given";

    parsed.valid = parsed.error.isEmpty();
    return parsed;
}

void disableCoreDumps()
{
   #if defined(RLIMIT_CORE)
    // A crashing plugin is expected traffic here; do not litter the working
    // directory with multi-gigabyte cores.
    struct rlimit limit
    {
        0, 0
    };

    setrlimit (RLIMIT_CORE, &limit);
   #endif
}

void printUsage()
{
    std::cerr << "usage: ihub-scanner --scan <bundle.vst3> --out <result.xml>"
                 " [--format VST3]" << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    ihub::plugins::capFileDescriptorLimit();
    disableCoreDumps();

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const Arguments arguments = parseArguments (juce::StringArray (argv, argc));

    if (! arguments.valid)
    {
        std::cerr << "ihub-scanner: " << arguments.error << std::endl;
        printUsage();
        return 2;
    }

    if (! arguments.scanFile.exists())
    {
        std::cerr << "ihub-scanner: no such bundle: "
                  << arguments.scanFile.getFullPathName() << std::endl;
        return 2;
    }

    juce::AudioPluginFormatManager formatManager;
    juce::addHeadlessDefaultFormatsToManager (formatManager);

    juce::AudioPluginFormat* format = nullptr;

    for (auto* candidate : formatManager.getFormats())
        if (candidate->getName() == arguments.format)
            format = candidate;

    if (format == nullptr)
    {
        std::cerr << "ihub-scanner: format not available: " << arguments.format << std::endl;
        return 3;
    }

    // The only place a plugin binary is ever loaded. If it takes the process
    // down, the parent notices the missing result file.
    juce::OwnedArray<juce::PluginDescription> found;
    format->findAllTypesForFile (found, arguments.scanFile.getFullPathName());

    // Same document shape as KnownPluginList::createXml(): a KNOWNPLUGINS
    // element with one child per PluginDescription. Building it by hand keeps
    // this helper on the headless JUCE module (no GUI/host classes needed).
    juce::XmlElement list ("KNOWNPLUGINS");

    for (auto* description : found)
        if (description != nullptr)
            list.addChildElement (description->createXml().release());

    if (! list.writeTo (arguments.outFile))
    {
        std::cerr << "ihub-scanner: could not write "
                  << arguments.outFile.getFullPathName() << std::endl;
        return 3;
    }

    std::cout << "ihub-scanner: " << found.size() << " type(s) in "
              << arguments.scanFile.getFullPathName() << std::endl;

    return 0;
}
