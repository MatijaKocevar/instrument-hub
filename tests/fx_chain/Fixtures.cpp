#include "Fixtures.h"

#include <cstring>
#include <iostream>

namespace FxChainTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool buffersBitEqual (const juce::AudioBuffer<float>& a,
                      const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels()
        || a.getNumSamples() != b.getNumSamples())
        return false;

    const auto bytes = static_cast<std::size_t> (a.getNumSamples())
                       * sizeof (float);

    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        if (std::memcmp (a.getReadPointer (channel),
                         b.getReadPointer (channel),
                         bytes) != 0)
            return false;

    return true;
}

void PatternSource::prepare (double, int maximumBlockSize)
{
    pattern.resize (static_cast<std::size_t> (juce::jmax (1, maximumBlockSize)));

    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
        const auto value = static_cast<int> ((i * 37u) % 200u) - 100;
        pattern[i] = static_cast<float> (value) / 250.0f;
    }
}

void PatternSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int samples = juce::jmin (buffer.getNumSamples(),
                                    static_cast<int> (pattern.size()));

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < samples; ++i)
            buffer.getWritePointer (channel)[i] = pattern[static_cast<std::size_t> (i)];
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

}
