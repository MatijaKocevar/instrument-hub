#include "Fixtures.h"

#include <algorithm>
#include <iostream>

namespace WarmPoolTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

void InstanceCounter::noteCreated()
{
    const int now = live.fetch_add (1, std::memory_order_relaxed) + 1;

    if (now > limit)
        violations.fetch_add (1, std::memory_order_relaxed);

    int previous = peak.load (std::memory_order_relaxed);

    while (now > previous
           && ! peak.compare_exchange_weak (previous, now,
                                            std::memory_order_relaxed))
    {
    }
}

void InstanceCounter::noteDestroyed()
{
    live.fetch_sub (1, std::memory_order_relaxed);
}

void InstanceCounter::resetPeak()
{
    peak.store (live.load (std::memory_order_relaxed), std::memory_order_relaxed);
    violations.store (0, std::memory_order_relaxed);
}

CountedInstance::CountedInstance (
    InstanceCounter& counterToUse,
    std::unique_ptr<ihub::plugins::PluginInstrument> instrumentToOwn)
    : counter (counterToUse), instrument (std::move (instrumentToOwn))
{
    counter.noteCreated();
}

CountedInstance::~CountedInstance()
{
    instrument.reset();
    counter.noteDestroyed();
}

void CountedInstance::prepare (double sampleRate, int maximumBlockSize)
{
    if (instrument != nullptr)
        instrument->prepare (sampleRate, maximumBlockSize);
}

void CountedInstance::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (instrument != nullptr)
        instrument->process (buffer, midi);
}

int CountedInstance::latencySamples() const noexcept
{
    return instrument != nullptr ? instrument->latencySamples() : 0;
}

double toMilliseconds (std::chrono::steady_clock::duration duration)
{
    return std::chrono::duration<double, std::milli> (duration).count();
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

bool contains (const std::vector<int>& values, int value)
{
    return std::find (values.begin(), values.end(), value) != values.end();
}

bool pumpUntil (const std::function<bool()>& predicate, int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds (timeoutMs);

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;

        juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
    }

    return predicate();
}

std::string join (const std::vector<int>& values)
{
    std::string text;

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
            text += ", ";

        text += std::to_string (values[i]);
    }

    return text;
}

} // namespace WarmPoolTest
