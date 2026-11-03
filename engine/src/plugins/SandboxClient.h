#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ihub::plugins
{

namespace sandbox
{

inline constexpr std::uint32_t sharedMemoryMagic = 0x49485553u;
inline constexpr std::uint32_t sharedMemoryVersion = 2;

inline constexpr int requestRingSize = 8;
inline constexpr int replyRingSize = 8;

inline constexpr int maximumBlockSize = 4096;
inline constexpr int maximumChannels = 2;
inline constexpr int maximumMidiEvents = 256;
inline constexpr int errorTextSize = 256;
inline constexpr int statePathSize = 512;
inline constexpr int maximumParameterBatch = 100000;

enum class ProcessState : std::int32_t
{
    empty = 0,
    loading = 1,
    ready = 2,
    stopping = 3,
    failed = 4
};

enum class ControlCommand : std::int32_t
{
    none = 0,
    applyState = 1,
    captureState = 2,
    applyParameters = 3
};

enum class ControlStatus : std::int32_t
{
    idle = 0,
    pending = 1,
    done = 2,
    failed = 3
};

struct SharedMidiEvent
{
    std::uint8_t bytes[3];
    std::uint8_t numBytes;
    std::int32_t sampleOffset;
};

struct SharedRequest
{
    std::uint32_t sequence;
    std::int32_t numSamples;
    std::uint32_t numMidiEvents;
    SharedMidiEvent midi[maximumMidiEvents];
};

struct SharedReply
{
    std::uint32_t sequence;
    std::int32_t numSamples;
    std::int32_t latencySamples;
    std::uint32_t flags;
    float audio[maximumChannels][maximumBlockSize];
};

struct SharedControl
{
    std::int32_t command;
    std::int32_t status;
    std::uint32_t sequence;
    std::int32_t stateBytes;
    char statePath[statePathSize];
};

struct SharedMemory
{
    std::uint32_t magic;
    std::uint32_t version;

    double sampleRate;
    std::int32_t maximumBlockSize;
    std::int32_t numChannels;

    std::int32_t state;
    std::int32_t parentProcessId;
    std::int32_t childProcessId;
    std::int32_t latencySamples;
    std::uint32_t heartbeat;

    std::uint64_t requestPublished;
    std::uint64_t requestConsumed;

    std::uint64_t replyPublished;

    char error[errorTextSize];

    SharedControl control;

    SharedRequest requests[requestRingSize];
    SharedReply replies[replyRingSize];
};

inline std::uint64_t loadAcquire (const std::uint64_t& value) noexcept
{
    return std::atomic_ref<std::uint64_t> (const_cast<std::uint64_t&> (value))
        .load (std::memory_order_acquire);
}

inline std::int32_t loadAcquire (const std::int32_t& value) noexcept
{
    return std::atomic_ref<std::int32_t> (const_cast<std::int32_t&> (value))
        .load (std::memory_order_acquire);
}

inline std::uint32_t loadAcquire (const std::uint32_t& value) noexcept
{
    return std::atomic_ref<std::uint32_t> (const_cast<std::uint32_t&> (value))
        .load (std::memory_order_acquire);
}

inline std::uint32_t loadRelaxed (const std::uint32_t& value) noexcept
{
    return std::atomic_ref<std::uint32_t> (const_cast<std::uint32_t&> (value))
        .load (std::memory_order_relaxed);
}

inline std::int32_t loadRelaxed (const std::int32_t& value) noexcept
{
    return std::atomic_ref<std::int32_t> (const_cast<std::int32_t&> (value))
        .load (std::memory_order_relaxed);
}

inline void storeRelease (std::uint64_t& value, std::uint64_t newValue) noexcept
{
    std::atomic_ref<std::uint64_t> (value).store (newValue, std::memory_order_release);
}

inline void storeRelease (std::int32_t& value, std::int32_t newValue) noexcept
{
    std::atomic_ref<std::int32_t> (value).store (newValue, std::memory_order_release);
}

inline void storeRelease (std::uint32_t& value, std::uint32_t newValue) noexcept
{
    std::atomic_ref<std::uint32_t> (value).store (newValue, std::memory_order_release);
}

inline void storeRelaxed (std::int32_t& value, std::int32_t newValue) noexcept
{
    std::atomic_ref<std::int32_t> (value).store (newValue, std::memory_order_relaxed);
}

inline void storeRelaxed (std::uint32_t& value, std::uint32_t newValue) noexcept
{
    std::atomic_ref<std::uint32_t> (value).store (newValue, std::memory_order_relaxed);
}

static_assert (std::atomic_ref<std::uint64_t>::is_always_lock_free,
               "the shared-memory counters must be lock-free for the audio thread");
static_assert (std::atomic_ref<std::int32_t>::is_always_lock_free,
               "the shared-memory state must be lock-free");
static_assert (std::atomic_ref<std::uint32_t>::is_always_lock_free,
               "the shared-memory control sequence must be lock-free");
static_assert (std::is_standard_layout_v<SharedMemory>,
               "the shared layout must be standard-layout");
static_assert (offsetof (SharedMemory, requestPublished) % alignof (std::uint64_t) == 0);
static_assert (offsetof (SharedMemory, replies) % alignof (std::uint32_t) == 0);

namespace cli
{
inline constexpr const char* helperName = "ihub-sandbox";
inline constexpr const char* mapFlag = "--map";
inline constexpr const char* pluginFlag = "--plugin";
inline constexpr const char* fakeToneFlag = "--fake-tone";
inline constexpr const char* abortAfterFlag = "--abort-after";
inline constexpr const char* paramFlag = "--param";
}

}

class SandboxClient final
{
public:
    static int run (const juce::StringArray& commandLine);

private:
    SandboxClient() = delete;
};

}
