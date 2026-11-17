#include "../PageHost.h"

#include <algorithm>
#include <cmath>

namespace ihub::plugins {
namespace {
constexpr int minimumMidiChannel = 1;
constexpr int maximumMidiChannel = 16;
constexpr int maximumMidiNote = 127;

juce::uint8 channelStatus(juce::uint8 baseStatus, int channel) noexcept {
    const auto clamped = juce::jlimit(minimumMidiChannel, maximumMidiChannel, channel);
    return static_cast<juce::uint8>(baseStatus | (clamped - 1));
}

}  // namespace

bool PageHost::pushPendingEvent(SlotState& slot, const std::uint8_t* bytes, int numBytes) noexcept {
    if (bytes == nullptr || numBytes <= 0 || numBytes > 3) return false;

    const auto write = slot.pendingWriteIndex.load(std::memory_order_relaxed);
    const auto next = (write + 1) % maxPendingMidiEvents;

    if (next == slot.pendingReadIndex.load(std::memory_order_acquire)) {
        slot.droppedMidiEvents.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    auto& event = slot.pendingMidi[write];

    for (int i = 0; i < numBytes; ++i) event.bytes[i] = bytes[i];

    for (int i = numBytes; i < 3; ++i) event.bytes[i] = 0;

    event.numBytes = static_cast<std::uint8_t>(numBytes);
    slot.pendingWriteIndex.store(next, std::memory_order_release);
    return true;
}

void PageHost::noteOn(int slot, int channel, int note, float velocity) {
    auto* state = findSlot(slot);

    if (state == nullptr) return;

    if (velocity <= 0.0f) {
        noteOff(slot, channel, note);
        return;
    }

    const auto value = static_cast<std::uint8_t>(
        juce::jlimit(1, 127, static_cast<int>(std::lround(velocity * 127.0f))));

    const std::uint8_t message[3]{channelStatus(0x90, channel),
                                  static_cast<std::uint8_t>(juce::jlimit(0, maximumMidiNote, note)),
                                  value};

    (void)pushPendingEvent(*state, message, 3);
}

void PageHost::noteOff(int slot, int channel, int note) {
    auto* state = findSlot(slot);

    if (state == nullptr) return;

    const std::uint8_t message[3]{channelStatus(0x80, channel),
                                  static_cast<std::uint8_t>(juce::jlimit(0, maximumMidiNote, note)),
                                  0};

    (void)pushPendingEvent(*state, message, 3);
}

void PageHost::allNotesOff(int slot) {
    auto* state = findSlot(slot);

    if (state == nullptr) return;

    for (int channel = 1; channel <= 16; ++channel) {
        const std::uint8_t noteOff[3]{channelStatus(0xB0, channel), 123, 0};
        const std::uint8_t soundOff[3]{channelStatus(0xB0, channel), 120, 0};
        const std::uint8_t sustainOff[3]{channelStatus(0xB0, channel), 64, 0};

        (void)pushPendingEvent(*state, noteOff, 3);
        (void)pushPendingEvent(*state, soundOff, 3);
        (void)pushPendingEvent(*state, sustainOff, 3);
    }
}

void PageHost::prepareSlot(int slot, double sampleRate, int maximumBlockSize) {
    preparedBlockSize.store(juce::jmax(1, maximumBlockSize), std::memory_order_relaxed);

    auto* state = findSlot(slot);

    if (state == nullptr || !isReady()) return;

    if (std::abs(sampleRate - options.sampleRate) > 0.5 || maximumBlockSize <= 0 ||
        maximumBlockSize > options.maxBlockSize) {
        const auto reason =
            "engine prepared the shared plugin page with an incompatible "
            "sample rate or block size";

        {
            const std::lock_guard<std::mutex> lock(state->stateLock);
            state->localError = reason;
        }

        state->failed.store(true, std::memory_order_relaxed);
        markStatus(Status::failed, reason);
    }
}

void PageHost::processSlot(int slot, int internalIndex, juce::AudioBuffer<float>& buffer,
                           juce::MidiBuffer& midi) {
    juce::ignoreUnused(slot);

    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0) return;

    buffer.clear();

    if (status.load(std::memory_order_acquire) != Status::ready) return;

    auto* shared = getSlotChannel(internalIndex);

    if (shared == nullptr) return;

    auto* state = (internalIndex >= 0 && internalIndex < static_cast<int>(slots.size()))
                      ? slots[static_cast<std::size_t>(internalIndex)].get()
                      : nullptr;

    if (state == nullptr || state->failed.load(std::memory_order_relaxed)) return;

    const int blockSamples = juce::jmin(numSamples, options.maxBlockSize);

    if (blockSamples <= 0) return;

    const auto published = sandbox::loadAcquire(shared->requestPublished);
    const auto consumed = sandbox::loadAcquire(shared->requestConsumed);

    if (published - consumed < static_cast<std::uint64_t>(sandbox::requestRingSize)) {
        auto& request = shared->requests[published % sandbox::requestRingSize];

        request.sequence = static_cast<std::uint32_t>(published);
        request.numSamples = blockSamples;

        std::uint32_t eventCount = 0;

        const auto addEvent = [&request, &eventCount, blockSamples](const std::uint8_t* bytes,
                                                                    int numBytes,
                                                                    int sampleOffset) noexcept {
            if (bytes == nullptr || numBytes <= 0) return false;

            if (eventCount >= static_cast<std::uint32_t>(sandbox::maximumMidiEvents)) return false;

            auto& event = request.midi[eventCount++];
            const auto bytesToCopy = juce::jmin(numBytes, 3);

            for (int i = 0; i < bytesToCopy; ++i) event.bytes[i] = bytes[i];

            for (int i = bytesToCopy; i < 3; ++i) event.bytes[i] = 0;

            event.numBytes = static_cast<std::uint8_t>(bytesToCopy);
            event.sampleOffset = juce::jlimit(0, juce::jmax(0, blockSamples - 1), sampleOffset);

            return true;
        };

        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();

            if (!addEvent(message.getRawData(), message.getRawDataSize(),
                          metadata.samplePosition)) {
                state->droppedMidiEvents.fetch_add(1, std::memory_order_relaxed);
            }
        }

        auto read = state->pendingReadIndex.load(std::memory_order_relaxed);
        const auto write = state->pendingWriteIndex.load(std::memory_order_acquire);

        while (read != write) {
            const auto& event = state->pendingMidi[read];

            if (!addEvent(event.bytes, event.numBytes, 0))
                state->droppedMidiEvents.fetch_add(1, std::memory_order_relaxed);

            read = (read + 1) % maxPendingMidiEvents;
        }

        state->pendingReadIndex.store(read, std::memory_order_release);

        request.numMidiEvents = eventCount;
        sandbox::storeRelease(shared->requestPublished, published + 1);
    } else {
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            (void)pushPendingEvent(*state, message.getRawData(), message.getRawDataSize());
        }

        state->droppedRequests.fetch_add(1, std::memory_order_relaxed);
    }

    auto repliesAvailable = sandbox::loadAcquire(shared->replyPublished);

    if (repliesAvailable > state->repliesPlayed &&
        repliesAvailable - state->repliesPlayed >
            static_cast<std::uint64_t>(sandbox::replyRingSize)) {
        const auto skipped = repliesAvailable - state->repliesPlayed -
                             static_cast<std::uint64_t>(sandbox::replyRingSize);

        state->underruns.fetch_add(static_cast<int>(juce::jmin<std::uint64_t>(skipped, 1000000)),
                                   std::memory_order_relaxed);
        state->repliesPlayed =
            repliesAvailable - static_cast<std::uint64_t>(sandbox::replyRingSize);
    }

    if (repliesAvailable > state->repliesPlayed) {
        const auto& reply = shared->replies[state->repliesPlayed % sandbox::replyRingSize];

        if (reply.sequence == static_cast<std::uint32_t>(state->repliesPlayed)) {
            const auto samples = juce::jlimit(0, numSamples, reply.numSamples);
            const auto channels = juce::jmin(buffer.getNumChannels(), sandbox::maximumChannels);

            for (int channel = 0; channel < channels; ++channel)
                buffer.copyFrom(channel, 0, reply.audio[channel], samples);

            if (reply.latencySamples >= 0)
                state->latencySamples.store(reply.latencySamples, std::memory_order_relaxed);

            state->playedBlocks.fetch_add(1, std::memory_order_relaxed);
        } else {
            state->underruns.fetch_add(1, std::memory_order_relaxed);
        }

        ++state->repliesPlayed;
    } else {
        state->underruns.fetch_add(1, std::memory_order_relaxed);
    }
}

int PageHost::getSlotLatencySamples(int slot) const noexcept {
    const auto* state = findSlot(slot);
    const auto plugin =
        state != nullptr ? juce::jmax(0, state->latencySamples.load(std::memory_order_relaxed)) : 0;

    if (!isReady()) return plugin;

    const auto block = preparedBlockSize.load(std::memory_order_relaxed);
    return plugin + (block > 0 ? block : options.maxBlockSize);
}

int PageHost::getSlotPluginLatencySamples(int slot) const noexcept {
    const auto* state = findSlot(slot);
    return state != nullptr ? juce::jmax(0, state->latencySamples.load(std::memory_order_relaxed))
                            : 0;
}

int PageHost::getSlotUnderrunCount(int slot) const noexcept {
    const auto* state = findSlot(slot);
    return state != nullptr ? state->underruns.load(std::memory_order_relaxed) : 0;
}

int PageHost::getSlotDroppedRequestCount(int slot) const noexcept {
    const auto* state = findSlot(slot);
    return state != nullptr ? state->droppedRequests.load(std::memory_order_relaxed) : 0;
}

int PageHost::getSlotDroppedMidiEventCount(int slot) const noexcept {
    const auto* state = findSlot(slot);
    return state != nullptr ? state->droppedMidiEvents.load(std::memory_order_relaxed) : 0;
}

std::int64_t PageHost::getSlotPlayedBlockCount(int slot) const noexcept {
    const auto* state = findSlot(slot);
    return state != nullptr ? state->playedBlocks.load(std::memory_order_relaxed) : 0;
}

}  // namespace ihub::plugins
