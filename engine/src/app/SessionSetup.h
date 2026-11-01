#pragma once

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub
{
class Session;
}

namespace ihub::app
{

std::unique_ptr<ihub::Session> startSession (const juce::String& commandLine);

}
