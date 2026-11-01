#include "TestSupport.h"

#include <juce_core/juce_core.h>

#include <iostream>

namespace DspTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string number (double value)
{
    juce::String text (value, 3);
    text = text.trimCharactersAtEnd ("0");
    text = text.trimCharactersAtEnd (".");

    return text.toStdString();
}

}
