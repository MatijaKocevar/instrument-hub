#include "multipatch_batch/ChildRunner.h"

#include <iostream>

namespace MultiPatch
{

ChildResult runChild (const juce::StringArray& arguments, int timeoutMs)
{
    ChildResult result;

    juce::ChildProcess process;

    if (! process.start (arguments,
                         juce::ChildProcess::wantStdOut
                             | juce::ChildProcess::wantStdErr))
    {
        result.output = "could not start " + arguments[0];
        return result;
    }

    result.timedOut = ! process.waitForProcessToFinish (timeoutMs);

    if (result.timedOut)
    {
        process.kill();
        (void) process.waitForProcessToFinish (1000);
        return result;
    }

    result.exitCode = static_cast<int> (process.getExitCode());
    result.output = process.readAllProcessOutput();
    return result;
}

void printChildSummary (const juce::String& output)
{
    juce::StringArray lines;
    lines.addLines (output);

    for (const auto& line : lines)
        if (line.contains ("ok  :") || line.contains ("FAIL:") || line.contains ("PASS:")
            || line.contains ("SKIP") || line.contains ("batch rig"))
            std::cout << line.toStdString() << std::endl;
}

}
