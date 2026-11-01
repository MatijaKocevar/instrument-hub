#include "TestSupport.h"

#include <iostream>

namespace DiagTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool contains (const std::string& text, const std::string& needle)
{
    return text.find (needle) != std::string::npos;
}

juce::File tempFile (const juce::String& suffix)
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("ihub-diagnostics", suffix, false);
}

} // namespace DiagTest
