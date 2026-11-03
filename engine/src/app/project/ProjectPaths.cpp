#include "ProjectPaths.h"

#include "app/CommandLine.h"

namespace ihub::app
{

juce::File defaultProjectFile()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Music")
        .getChildFile ("Instrument Hub")
        .getChildFile ("Untitled.ihub");
}

juce::File resolveProjectFile (const juce::String& commandLine)
{
    auto value = getCommandLineValue (commandLine, "--project");

    if (value.isEmpty())
        return defaultProjectFile();

    if (value == "~")
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    if (value.startsWith ("~/"))
        value = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                    .getChildFile (value.substring (2))
                    .getFullPathName();

    if (juce::File::isAbsolutePath (value))
        return juce::File (value);

    return juce::File::getCurrentWorkingDirectory().getChildFile (value);
}

}
