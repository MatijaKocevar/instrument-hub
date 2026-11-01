#include "../SamplerInstrument.h"

#include "SamplerInstrumentConstants.h"

#include <cmath>

namespace ihub::sampler
{

using namespace instrument_detail;

void SamplerInstrument::noteOn (int channel, int note, float velocity)
{
    if (velocity <= 0.0f)
    {
        noteOff (channel, note);
        return;
    }

    const auto clampedChannel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);
    const auto clampedNote = clampNote (note);
    const auto midiVelocity = juce::jlimit (
        midiVelocityMinimum, midiVelocityMaximum,
        (int) std::lround (velocity * (float) midiVelocityMaximum));

    const std::uint8_t message[maximumMidiBytes] {
        static_cast<std::uint8_t> (0x90 | (clampedChannel - 1)),
        static_cast<std::uint8_t> (clampedNote),
        static_cast<std::uint8_t> (midiVelocity)
    };

    pushEvent (message, maximumMidiBytes);
}

void SamplerInstrument::noteOff (int channel, int note)
{
    const auto clampedChannel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);

    const std::uint8_t message[maximumMidiBytes] {
        static_cast<std::uint8_t> (0x80 | (clampedChannel - 1)),
        static_cast<std::uint8_t> (clampNote (note)),
        0
    };

    pushEvent (message, maximumMidiBytes);
}

void SamplerInstrument::allNotesOff (int channel)
{
    const auto clampedChannel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);

    const std::uint8_t message[maximumMidiBytes] {
        static_cast<std::uint8_t> (0xB0 | (clampedChannel - 1)),
        123,
        0
    };

    pushEvent (message, maximumMidiBytes);
}

bool SamplerInstrument::pushEvent (const std::uint8_t* bytes, int numBytes) noexcept
{
    const auto write = pendingWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1) % maxPendingEvents;

    if (next == pendingReadIndex.load (std::memory_order_acquire))
    {
        droppedEventCount.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    auto& event = pendingEvents[write];

    for (int i = 0; i < numBytes; ++i)
        event.bytes[i] = bytes[i];

    event.numBytes = static_cast<std::uint8_t> (juce::jlimit (0, maximumMidiBytes, numBytes));

    pendingWriteIndex.store (next, std::memory_order_release);
    return true;
}

int SamplerInstrument::getPendingEventCount() const noexcept
{
    const auto read = pendingReadIndex.load (std::memory_order_acquire);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    return (int) ((write + maxPendingEvents - read) % maxPendingEvents);
}

void SamplerInstrument::drainQueuedEvents() noexcept
{
    auto read = pendingReadIndex.load (std::memory_order_relaxed);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        applyQueuedEvent (pendingEvents[read]);
        read = (read + 1) % maxPendingEvents;
    }

    pendingReadIndex.store (read, std::memory_order_release);
}

void SamplerInstrument::applyQueuedEvent (const PendingEvent& event) noexcept
{
    if (event.numBytes < 2)
        return;

    const auto status = event.bytes[0];
    const auto type = status & 0xF0;
    const auto channel = (int) (status & 0x0F) + 1;

    if (type == 0x90 || type == 0x80)
    {
        if (event.numBytes < 3)
            return;

        const auto note = (int) event.bytes[1];
        const auto velocity = (int) event.bytes[2];

        if (type == 0x90 && velocity > 0)
            startVoice (channel, note, velocity);
        else
            releaseVoices (channel, note);

        return;
    }

    if (type == 0xB0 && event.numBytes >= 3)
    {
        if (event.bytes[1] == 123 || event.bytes[1] == 120)
            allNotesOffNow();
    }
}

void SamplerInstrument::applyMidiEvent (const juce::MidiMessageMetadata& metadata) noexcept
{
    if (metadata.data == nullptr || metadata.numBytes <= 0)
        return;

    PendingEvent event;
    event.numBytes = static_cast<std::uint8_t> (
        juce::jmin (maximumMidiBytes, metadata.numBytes));

    for (int i = 0; i < (int) event.numBytes; ++i)
        event.bytes[i] = metadata.data[i];

    applyQueuedEvent (event);
}

}
