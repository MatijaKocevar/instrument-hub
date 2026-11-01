#include "analog_lab/ChildProcess.h"
#include "analog_lab/Constants.h"

#include <cstdlib>
#include <iostream>

#if defined(__linux__)
 #include <sys/wait.h>
#endif

namespace AnalogLab
{

namespace
{
void printChildSummary (const juce::String& output)
{
    juce::StringArray lines;
    lines.addLines (output);

    for (const auto& line : lines)
    {
        if (line.contains ("ok  :") || line.contains ("FAIL:")
            || line.contains ("PASS:") || line.contains ("AUDIO")
            || line.contains ("SKIP") || line.contains ("stack overflow"))
        {
            std::cout << line.toStdString() << std::endl;
        }
    }
}
} // namespace

int runChild (const juce::File& executable,
              const juce::StringArray& arguments,
              const juce::File& logFile,
              bool printSummary)
{
    auto quote = [] (const juce::String& value)
    {
        return "\"" + value.replace ("\"", "\\\"") + "\"";
    };

    juce::String command = "/usr/bin/timeout --kill-after=5 "
                         + juce::String (kChildTimeoutSeconds) + " "
                         + quote (executable.getFullPathName());

    for (const auto& argument : arguments)
        command += " " + quote (argument);

    command += " > " + quote (logFile.getFullPathName()) + " 2>&1";

    const int status = std::system (command.toRawUTF8());
    int exitCode = -1;

   #if defined(__linux__)
    if (WIFEXITED (status))
        exitCode = WEXITSTATUS (status);
    else if (WIFSIGNALED (status))
        exitCode = 128 + WTERMSIG (status);
   #else
    exitCode = status;
   #endif

    const auto output = logFile.existsAsFile() ? logFile.loadFileAsString()
                                               : juce::String();

    if (printSummary)
        printChildSummary (output);
    else if (exitCode != 0)
        std::cout << output.toStdString() << std::endl;

    return exitCode;
}

int runChildWithRetry (const juce::File& executable,
                       const juce::StringArray& arguments,
                       const juce::File& logFile,
                       const juce::String& label)
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const int exitCode = runChild (executable, arguments, logFile, true);

        if (exitCode != 124)
            return exitCode;

        std::cout << "note: " << label << " timed out after "
                  << kChildTimeoutSeconds << " s; retrying once" << std::endl;
    }

    return 124;
}

} // namespace AnalogLab
