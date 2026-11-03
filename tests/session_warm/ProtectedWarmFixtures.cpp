#include "ProtectedWarmFixtures.h"

#include <juce_events/juce_events.h>

#include <iostream>

namespace ProtectedWarmTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::vector<int> slotRange (int count)
{
    std::vector<int> slots;

    for (int index = 0; index < count; ++index)
        slots.push_back (index);

    return slots;
}

bool pumpUntil (const std::function<bool()>& predicate, int timeoutMs)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (timeoutMs);

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (predicate())
            return true;

        juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
    }

    return predicate();
}

CountingSource::CountingSource (SourceCounters& countersToUse)
    : counters (countersToUse)
{
    ++counters.created;
    ++counters.live;
}

CountingSource::~CountingSource()
{
    --counters.live;
}

void CountingSource::prepare (double, int) {}

void CountingSource::process (juce::AudioBuffer<float>&, juce::MidiBuffer&) {}

int CountingSource::latencySamples() const noexcept
{
    return 0;
}

ihub::SessionConfig makeConfig()
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.initialSlot = 0;
    config.neighbourCount = 2;
    config.warmSlots = slotRange (kSlots);
    config.loadDelayMilliseconds = 5;
    config.slots.resize (kSlots);

    for (int index = 0; index < kSlots; ++index)
    {
        auto& slot = config.slots[static_cast<std::size_t> (index)];
        slot.kind = ihub::SessionSlotKind::initTone;
        slot.name = "track-" + std::to_string (index);
        slot.frequency = 110.0 * static_cast<double> (index + 1);
    }

    return config;
}

}
