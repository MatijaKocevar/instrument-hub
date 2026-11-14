#include "ProcessLimits.h"
#include "ScannerProcess.h"

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
        juce::String format{ihub::plugins::scanner_cli::defaultFormat};
        bool valid = false;
        juce::String error;
    };

    Arguments parseArguments (const juce::StringArray& arguments)
    {
        Arguments parsed;

        for (int i = 1; i < arguments.size (); ++i)
        {
            const auto& argument = arguments[i];

            auto takeValue = [&arguments, &i, &parsed] (juce::File& target) -> bool
            {
                if (i + 1 >= arguments.size ())
                {
                    parsed.error = "missing value after " + arguments[i];
                    return false;
                }

                target = juce::File (juce::String (arguments[++i])).getFullPathName ();
                return true;
            };

            if (argument == ihub::plugins::scanner_cli::scanFlag)
            {
                if (!takeValue (parsed.scanFile))
                    return parsed;
            }
            else if (argument == ihub::plugins::scanner_cli::outFlag)
            {
                if (!takeValue (parsed.outFile))
                    return parsed;
            }
            else if (argument == ihub::plugins::scanner_cli::formatFlag)
            {
                if (i + 1 >= arguments.size ())
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

        if (parsed.scanFile == juce::File ())
            parsed.error = "no --scan path given";
        else if (parsed.outFile == juce::File ())
            parsed.error = "no --out path given";

        parsed.valid = parsed.error.isEmpty ();
        return parsed;
    }

    void disableCoreDumps ()
    {
#if defined(RLIMIT_CORE)
        struct rlimit limit{0, 0};

        setrlimit (RLIMIT_CORE, &limit);
#endif
    }

    void printUsage ()
    {
        std::cerr << "usage: ihub-scanner --scan <bundle.vst3> --out <result.xml>"
                     " [--format VST3]"
                  << std::endl;
    }
}

int main (int argc, char** argv)
{
    ihub::plugins::capFileDescriptorLimit ();
    disableCoreDumps ();

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const Arguments arguments = parseArguments (juce::StringArray (argv, argc));

    if (!arguments.valid)
    {
        std::cerr << "ihub-scanner: " << arguments.error << std::endl;
        printUsage ();
        return 2;
    }

    if (!arguments.scanFile.exists ())
    {
        std::cerr << "ihub-scanner: no such bundle: " << arguments.scanFile.getFullPathName ()
                  << std::endl;
        return 2;
    }

    juce::AudioPluginFormatManager formatManager;
    juce::addHeadlessDefaultFormatsToManager (formatManager);

    juce::AudioPluginFormat* format = nullptr;

    for (auto* candidate : formatManager.getFormats ())
        if (candidate->getName () == arguments.format)
            format = candidate;

    if (format == nullptr)
    {
        std::cerr << "ihub-scanner: format not available: " << arguments.format << std::endl;
        return 3;
    }

    juce::OwnedArray<juce::PluginDescription> found;
    format->findAllTypesForFile (found, arguments.scanFile.getFullPathName ());

    juce::XmlElement list ("KNOWNPLUGINS");

    for (auto* description : found)
        if (description != nullptr)
            list.addChildElement (description->createXml ().release ());

    if (!list.writeTo (arguments.outFile))
    {
        std::cerr << "ihub-scanner: could not write " << arguments.outFile.getFullPathName ()
                  << std::endl;
        return 3;
    }

    std::cout << "ihub-scanner: " << found.size () << " type(s) in "
              << arguments.scanFile.getFullPathName () << std::endl;

    return 0;
}
