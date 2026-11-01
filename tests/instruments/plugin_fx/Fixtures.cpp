#include "Fixtures.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>

namespace PluginFxTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool nearlyEqual (float a, float b, float tolerance)
{
    return std::abs (a - b) <= tolerance;
}

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types)
{
    const auto path = bundle.getFullPathName();

    for (auto* format : manager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    for (auto* type : types)
        if (type->name.contains ("TestPlugin"))
            return type;

    return nullptr;
}

void fillPattern (juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = static_cast<int> ((i * 13 + channel * 29) % 100) - 50;
            buffer.setSample (channel, i, static_cast<float> (value) / 100.0f);
        }
}

void pumpMessages (int milliseconds)
{
    auto* messageManager = juce::MessageManager::getInstance();

    if (messageManager == nullptr)
        return;

    const auto deadline = juce::Time::getMillisecondCounter()
                          + static_cast<juce::uint32> (juce::jmax (0, milliseconds));

    while (juce::Time::getMillisecondCounter() < deadline)
        messageManager->runDispatchLoopUntil (10);
}

}
