#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

#include <chrono>
#include <string>

namespace ShutdownTest
{

extern int failures;

using Clock = std::chrono::steady_clock;

void check (bool condition, const std::string& what);

double millisecondsSince (Clock::time_point start);

ihub::SessionConfig makeToneConfig (int slots);

bool waitForFile (const juce::File& file, int timeoutMs);

}
