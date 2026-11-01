#include "CommandLine.h"

namespace ihub::app
{

juce::String getCommandLineValue (const juce::String& commandLine,
                                  const juce::String& option)
{
    const auto tokens = juce::StringArray::fromTokens (commandLine, true);

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& token = tokens[i];

        if (token == option)
            return i + 1 < tokens.size() ? tokens[i + 1].unquoted() : juce::String();

        if (token.startsWith (option + "="))
            return token.substring (option.length() + 1).unquoted();
    }

    return {};
}

}
