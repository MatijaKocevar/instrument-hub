#include "../SandboxHost.h"

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

void SandboxHost::noteOn (int ch, int note, float vel)
{
    if (vel <= 0.0f)
    {
        noteOff (ch, note);
        return;
    }

    const auto velocity = static_cast<std::uint8_t> (juce::jlimit (
        1, 127, static_cast<int> (std::lround (vel * 127.0f))));

    const std::uint8_t message[3] {
        channelStatus (0x90, ch),
        static_cast<std::uint8_t> (juce::jlimit (0, maximumMidiNote, note)),
        velocity
    };

    pushEvent (message, 3);
}

void SandboxHost::noteOff (int ch, int note)
{
    const std::uint8_t message[3] {
        channelStatus (0x80, ch),
        static_cast<std::uint8_t> (juce::jlimit (0, maximumMidiNote, note)),
        0
    };

    pushEvent (message, 3);
}

bool SandboxHost::pushEvent (const std::uint8_t* bytes, int numBytes) noexcept
{
    if (bytes == nullptr || numBytes <= 0 || numBytes > 3)
        return false;

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

    event.numBytes = static_cast<std::uint8_t> (numBytes);

    pendingWriteIndex.store (next, std::memory_order_release);
    return true;
}

}
