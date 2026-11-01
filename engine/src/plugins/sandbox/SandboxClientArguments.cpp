#include "SandboxClientArguments.h"

#include "../SandboxClient.h"

namespace ihub::plugins::sandbox_client
{
ChildArguments parseArguments (const juce::StringArray& commandLine)
{
    ChildArguments arguments;

    auto takeString = [&commandLine] (int& index, juce::String& target) -> bool
    {
        if (index + 1 >= commandLine.size())
            return false;

        target = commandLine[++index];
        return true;
    };

    for (int i = 1; i < commandLine.size(); ++i)
    {
        const auto& argument = commandLine[i];

        if (argument == sandbox::cli::mapFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.mapFile = juce::File (value).getFullPathName();
        }
        else if (argument == sandbox::cli::pluginFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.pluginBundle = juce::File (value).getFullPathName();
        }
        else if (argument == sandbox::cli::fakeToneFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.fakeToneFrequency = value.getDoubleValue();

            if (arguments.fakeToneFrequency <= 0.0)
            {
                arguments.error = "invalid fake tone frequency: " + value;
                return arguments;
            }
        }
        else if (argument == sandbox::cli::abortAfterFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.abortAfterBlocks = value.getIntValue();

            if (arguments.abortAfterBlocks < 0)
            {
                arguments.error = "invalid abort-after block count: " + value;
                return arguments;
            }
        }
        else if (argument == sandbox::cli::paramFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            const auto separator = value.indexOfChar ('=');

            if (separator <= 0 || separator + 1 >= value.length())
            {
                arguments.error = "invalid parameter assignment (expected name=value): " + value;
                return arguments;
            }

            ParameterAssignment assignment;
            assignment.name = value.substring (0, separator).trim();
            assignment.value = value.substring (separator + 1).trim().getFloatValue();
            arguments.parameters.push_back (assignment);
        }
        else
        {
            arguments.error = "unknown argument: " + argument;
            return arguments;
        }
    }

    if (arguments.mapFile == juce::File())
        arguments.error = "no --map path given";
    else if (arguments.pluginBundle == juce::File() && arguments.fakeToneFrequency <= 0.0)
        arguments.error = "no --plugin bundle and no --fake-tone frequency given";

    arguments.valid = arguments.error.isEmpty();
    return arguments;
}

} // namespace ihub::plugins::sandbox_client
