#include "TestSupport.h"

#include <iostream>

namespace ShutdownTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

double millisecondsSince (Clock::time_point start)
{
    return std::chrono::duration<double, std::milli> (Clock::now() - start).count();
}

ihub::SessionConfig makeToneConfig (int slots)
{
    ihub::SessionConfig config;
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 0;
    config.fadeMilliseconds = 3.0f;
    config.initialSlot = 0;
    config.neighbourCount = 2;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    config.slots.resize (static_cast<std::size_t> (slots));
    return config;
}

bool waitForFile (const juce::File& file, int timeoutMs)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds (timeoutMs);

    while (Clock::now() < deadline)
    {
        if (file.existsAsFile())
            return true;

        juce::Thread::sleep (2);
    }

    return file.existsAsFile();
}

}
