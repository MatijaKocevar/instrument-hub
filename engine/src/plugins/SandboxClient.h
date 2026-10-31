#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ihub::plugins
{
/**
    Wire protocol shared by `SandboxHost` (the process hosting the engine) and
    `SandboxClient` (the `ihub-sandbox` helper that owns the plugin).

    ## Why a file mapping and not a socket

    Audio must not cross a socket: a block is 128..2048 frames and the host
    callback can never wait for a packet. The host therefore maps one file
    (created in the system temp directory, deleted when the host stops) into
    both processes and exchanges fixed-size blocks through two lock-free SPSC
    rings:

        host audio thread  --request ring-->  sandbox thread (renders plugin)
        host audio thread  <--reply ring----  sandbox thread (audio frames)

    The host never blocks: when a reply is not ready the callback leaves the
    block silent and increments an underrun counter. When the sandbox dies the
    mapping stays valid, so the host keeps rendering silence for that slot and
    the rest of the graph is unaffected.

    ## Memory ordering

    Counters are plain integers updated through `std::atomic_ref`. Publishing a
    block stores a release view of the counter after the block has been filled;
    the reader loads it with acquire before touching the payload. Everything is
    lock-free on 64-bit targets (`static_assert`ed below), which is what makes
    the host's audio-thread access legal.

    The layout is versioned and deliberately uses only fixed-width integers and
    floats, so both processes agree byte for byte. `SharedMemory` is never
    constructed in place: hosts `memset` it and children attach to the bytes.
*/
namespace sandbox
{

inline constexpr std::uint32_t sharedMemoryMagic = 0x49485553u; // 'IHUS'
inline constexpr std::uint32_t sharedMemoryVersion = 1;

/** Blocks in each direction. Eight blocks of slack hide scheduling jitter;
    the host stops writing (and underruns) when the sandbox falls further
    behind than this. */
inline constexpr int requestRingSize = 8;
inline constexpr int replyRingSize = 8;

/** Capacities fixed at compile time so both processes have identical types.
    `SandboxHost` rejects a larger engine block size rather than truncating it.
*/
inline constexpr int maximumBlockSize = 4096;
inline constexpr int maximumChannels = 2;
inline constexpr int maximumMidiEvents = 256;
inline constexpr int errorTextSize = 256;

/** Lifecycle the sandbox publishes in the shared header. */
enum class ProcessState : std::int32_t
{
    empty = 0,
    loading = 1,
    ready = 2,
    stopping = 3,
    failed = 4
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

struct SharedMemory
{
    std::uint32_t magic;
    std::uint32_t version;

    double sampleRate;
    std::int32_t maximumBlockSize;
    std::int32_t numChannels;

    std::int32_t state;
    std::int32_t parentProcessId;
    std::int32_t latencySamples;
    std::uint32_t heartbeat;

    /** Requests written by the host and consumed by the sandbox. */
    std::uint64_t requestPublished;
    std::uint64_t requestConsumed;

    /** Replies written by the sandbox and played by the host. */
    std::uint64_t replyPublished;

    /** NUL-terminated failure text, written before `state` becomes failed. */
    char error[errorTextSize];

    SharedRequest requests[requestRingSize];
    SharedReply replies[replyRingSize];
};

//==============================================================================
// Atomic accessors for the shared payload. Overloads keep call sites readable.

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
static_assert (std::is_standard_layout_v<SharedMemory>,
               "the shared layout must be standard-layout");
static_assert (offsetof (SharedMemory, requestPublished) % alignof (std::uint64_t) == 0);
static_assert (offsetof (SharedMemory, replies) % alignof (std::uint32_t) == 0);

//==============================================================================
/** Command-line contract between SandboxHost and the ihub-sandbox helper. */
namespace cli
{
inline constexpr const char* helperName = "ihub-sandbox";
inline constexpr const char* mapFlag = "--map";
inline constexpr const char* pluginFlag = "--plugin";
inline constexpr const char* fakeToneFlag = "--fake-tone";
inline constexpr const char* abortAfterFlag = "--abort-after";
inline constexpr const char* paramFlag = "--param";
} // namespace cli

} // namespace sandbox

//==============================================================================
/**
    Child side of the sandbox: attaches to the shared mapping, loads exactly one
    plugin (or the built-in fake tone generator used by the tests), renders
    requested blocks until the host asks it to stop, then exits.

    Run only from the `ihub-sandbox` executable. The helper is intentionally
    dumb and single-purpose, exactly like `ihub-scanner`: a plugin that calls
    `abort()`, segfaults or hangs can only end this process. The host notices
    through its watchdog and marks the slot failed.
*/
class SandboxClient final
{
public:
    /** Entry point for `ihub-sandbox`; returns the process exit code. */
    static int run (const juce::StringArray& commandLine);

private:
    SandboxClient() = delete;
};

} // namespace ihub::plugins
