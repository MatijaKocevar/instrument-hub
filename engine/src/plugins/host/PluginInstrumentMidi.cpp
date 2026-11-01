#include "../PluginInstrument.h"

#include <algorithm>
#include <cmath>

namespace ihub::plugins
{
namespace
{
constexpr int minimumMidiChannel = 1;
constexpr int maximumMidiChannel = 16;
constexpr int maximumMidiNote = 127;

juce::uint8 channelStatus (juce::uint8 baseStatus, int channel) noexcept
{
    const auto clamped = juce::jlimit (minimumMidiChannel, maximumMidiChannel, channel);
    return static_cast<juce::uint8> (baseStatus | (clamped - 1));
}
}

void PluginInstrument::noteOn (int ch, int note, float vel)
{
    if (vel <= 0.0f)
    {
        noteOff (ch, note);
        return;
    }

    const auto velocity = static_cast<juce::uint8> (juce::jlimit (
        1, 127, static_cast<int> (std::lround (vel * 127.0f))));

    const juce::uint8 message[3] {
        channelStatus (0x90, ch),
        static_cast<juce::uint8> (juce::jlimit (0, maximumMidiNote, note)),
        velocity
    };

    pushEvent (message, 3);
}

void PluginInstrument::noteOff (int ch, int note)
{
    const juce::uint8 message[3] {
        channelStatus (0x80, ch),
        static_cast<juce::uint8> (juce::jlimit (0, maximumMidiNote, note)),
        0
    };

    pushEvent (message, 3);
}

bool PluginInstrument::queueMidi (const juce::uint8* bytes, int numBytes)
{
    if (bytes == nullptr || numBytes <= 0 || numBytes > 3)
        return false;

    return pushEvent (bytes, numBytes);
}

void PluginInstrument::allNotesOff (int ch)
{
    const juce::uint8 message[3] { channelStatus (0xB0, ch), 123, 0 };
    pushEvent (message, 3);
}

bool PluginInstrument::pushEvent (const juce::uint8* bytes, int numBytes) noexcept
{
    const auto write = pendingWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1) % maxPendingMidiEvents;

    if (next == pendingReadIndex.load (std::memory_order_acquire))
    {
        droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    auto& event = pendingMidi[write];

    for (int i = 0; i < numBytes; ++i)
        event.bytes[i] = bytes[i];

    for (int i = numBytes; i < 3; ++i)
        event.bytes[i] = 0;

    event.numBytes = static_cast<juce::uint8> (numBytes);

    pendingWriteIndex.store (next, std::memory_order_release);

    return true;
}

void PluginInstrument::drainPendingMidi (juce::MidiBuffer& destination) noexcept
{
    auto read = pendingReadIndex.load (std::memory_order_relaxed);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        const auto& event = pendingMidi[read];

        if (event.numBytes > 0)
        {
            destination.addEvent (event.bytes, event.numBytes, 0);
            deliveredMidiEvents.fetch_add (1, std::memory_order_relaxed);
        }

        read = (read + 1) % maxPendingMidiEvents;
    }

    pendingReadIndex.store (read, std::memory_order_release);
}

int PluginInstrument::getPendingMidiEventCount() const noexcept
{
    const auto read = pendingReadIndex.load (std::memory_order_acquire);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    return static_cast<int> ((write + maxPendingMidiEvents - read) % maxPendingMidiEvents);
}

int PluginInstrument::getDeliveredMidiEventCount() const noexcept
{
    return deliveredMidiEvents.load (std::memory_order_relaxed);
}

int PluginInstrument::getDroppedMidiEventCount() const noexcept
{
    return droppedMidiEvents.load (std::memory_order_relaxed);
}

void PluginInstrument::clearPendingMidi() noexcept
{
    pendingReadIndex.store (pendingWriteIndex.load (std::memory_order_acquire),
                            std::memory_order_release);
}

}
