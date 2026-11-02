#include "../SandboxHost.h"

#include <algorithm>
#include <cmath>

namespace ihub::plugins
{
void SandboxHost::prepare (double sampleRate, int maximumBlockSize)
{
    preparedBlockSize.store (juce::jmax (1, maximumBlockSize), std::memory_order_relaxed);

    if (status.load (std::memory_order_relaxed) != Status::running)
        return;

    if (std::abs (sampleRate - options.sampleRate) > 0.5
        || maximumBlockSize <= 0
        || maximumBlockSize > options.maxBlockSize)
    {
        markFailed ("engine prepared the sandbox with an incompatible sample rate or block size");
    }
}

void SandboxHost::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    buffer.clear();

    if (shared == nullptr
        || status.load (std::memory_order_relaxed) != Status::running
        || failed.load (std::memory_order_relaxed))
    {
        return;
    }

    const int blockSamples = juce::jmin (numSamples, options.maxBlockSize);

    if (blockSamples <= 0)
        return;

    const auto published = sandbox::loadAcquire (shared->requestPublished);
    const auto consumed = sandbox::loadAcquire (shared->requestConsumed);

    if (published - consumed < static_cast<std::uint64_t> (sandbox::requestRingSize))
    {
        auto& request = shared->requests[published % sandbox::requestRingSize];

        request.sequence = static_cast<std::uint32_t> (published);
        request.numSamples = blockSamples;

        std::uint32_t eventCount = 0;

        const auto addEvent = [&request, &eventCount, blockSamples] (const std::uint8_t* bytes,
                                                                     int numBytes,
                                                                     int sampleOffset) noexcept
        {
            if (bytes == nullptr || numBytes <= 0)
                return false;

            if (eventCount >= static_cast<std::uint32_t> (sandbox::maximumMidiEvents))
                return false;

            auto& event = request.midi[eventCount++];
            const auto bytesToCopy = juce::jmin (numBytes, 3);

            for (int i = 0; i < bytesToCopy; ++i)
                event.bytes[i] = bytes[i];

            for (int i = bytesToCopy; i < 3; ++i)
                event.bytes[i] = 0;

            event.numBytes = static_cast<std::uint8_t> (bytesToCopy);
            event.sampleOffset = juce::jlimit (0, juce::jmax (0, blockSamples - 1), sampleOffset);

            return true;
        };

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (! addEvent (message.getRawData(), message.getRawDataSize(),
                            metadata.samplePosition))
            {
                droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);
            }
        }

        auto read = pendingReadIndex.load (std::memory_order_relaxed);
        const auto write = pendingWriteIndex.load (std::memory_order_acquire);

        while (read != write)
        {
            const auto& event = pendingMidi[read];

            if (! addEvent (event.bytes, event.numBytes, 0))
                droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);

            read = (read + 1) % maxPendingMidiEvents;
        }

        pendingReadIndex.store (read, std::memory_order_release);

        request.numMidiEvents = eventCount;

        sandbox::storeRelease (shared->requestPublished, published + 1);
    }
    else
    {
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            (void) pushEvent (message.getRawData(), message.getRawDataSize());
        }

        droppedRequests.fetch_add (1, std::memory_order_relaxed);
    }

    auto repliesAvailable = sandbox::loadAcquire (shared->replyPublished);

    if (repliesAvailable > repliesPlayed
        && repliesAvailable - repliesPlayed > static_cast<std::uint64_t> (sandbox::replyRingSize))
    {
        const auto skipped = repliesAvailable - repliesPlayed
                           - static_cast<std::uint64_t> (sandbox::replyRingSize);

        underruns.fetch_add (static_cast<int> (juce::jmin<std::uint64_t> (skipped, 1000000)),
                             std::memory_order_relaxed);
        repliesPlayed = repliesAvailable - static_cast<std::uint64_t> (sandbox::replyRingSize);
    }

    if (repliesAvailable > repliesPlayed)
    {
        const auto& reply = shared->replies[repliesPlayed % sandbox::replyRingSize];

        if (reply.sequence == static_cast<std::uint32_t> (repliesPlayed))
        {
            const auto samples = juce::jlimit (0, numSamples, reply.numSamples);
            const auto channels = juce::jmin (buffer.getNumChannels(), sandbox::maximumChannels);

            for (int channel = 0; channel < channels; ++channel)
                buffer.copyFrom (channel, 0, reply.audio[channel], samples);

            if (reply.latencySamples >= 0)
                pluginLatency.store (reply.latencySamples, std::memory_order_relaxed);

            playedBlocks.fetch_add (1, std::memory_order_relaxed);
        }
        else
        {
            underruns.fetch_add (1, std::memory_order_relaxed);
        }

        ++repliesPlayed;
    }
    else
    {
        underruns.fetch_add (1, std::memory_order_relaxed);
    }
}

int SandboxHost::latencySamples() const noexcept
{
    const auto plugin = juce::jmax (0, pluginLatency.load (std::memory_order_relaxed));

    if (! isRunning())
        return plugin;

    const auto block = preparedBlockSize.load (std::memory_order_relaxed);
    return plugin + (block > 0 ? block : options.maxBlockSize);
}

int SandboxHost::getPluginLatencySamples() const noexcept
{
    return juce::jmax (0, pluginLatency.load (std::memory_order_relaxed));
}

int SandboxHost::getUnderrunCount() const noexcept
{
    return underruns.load (std::memory_order_relaxed);
}

int SandboxHost::getDroppedRequestCount() const noexcept
{
    return droppedRequests.load (std::memory_order_relaxed);
}

int SandboxHost::getDroppedMidiEventCount() const noexcept
{
    return droppedMidiEvents.load (std::memory_order_relaxed);
}

std::int64_t SandboxHost::getPlayedBlockCount() const noexcept
{
    return playedBlocks.load (std::memory_order_relaxed);
}

}
