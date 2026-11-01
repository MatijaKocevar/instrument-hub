#include "TestSupport.h"

#include "app/CommandLine.h"

#include <iostream>

namespace AppModesTest
{

int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

void checkValue (const juce::String& commandLine,
                 const juce::String& option,
                 const juce::String& expected)
{
    const auto actual = ihub::app::getCommandLineValue (commandLine, option);
    check (actual == expected,
           "value(" + commandLine + ", " + option + ") == \"" + expected
               + "\" (got \"" + actual + "\")");
}

void checkMode (const juce::String& commandLine, ihub::app::AppMode expected)
{
    const auto actual = ihub::app::selectMode (commandLine);
    check (actual == expected,
           "mode(" + commandLine + ") == " + ihub::app::toString (expected)
               + " (got " + ihub::app::toString (actual) + ")");
}

}
