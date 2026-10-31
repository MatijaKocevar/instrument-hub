#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>

namespace ihub::diag
{

/** Severity of a structured log record. */
enum class LogLevel : std::uint8_t
{
    trace = 0,
    debug,
    info,
    warning,
    error,
    fatal
};

/** Subsystem that produced a record. */
enum class LogSource : std::uint8_t
{
    general = 0,
    audio,
    midi,
    controller,
    api,
    plugins,
    project,
    sampler,
    ui,
    diagnostics
};

const char* toString (LogLevel level) noexcept;
const char* toString (LogSource source) noexcept;

bool parseLogLevel (std::string_view text, LogLevel& level) noexcept;
bool parseLogSource (std::string_view text, LogSource& source) noexcept;

inline constexpr std::size_t logMessageCapacity = 240;
inline constexpr std::size_t defaultLogCapacity = 1024;

/** One fixed-size, trivially-copyable log record.

    `code` is the structured part: a stable integer event id chosen by the
    subsystem (0 means "plain message"). `sequence` starts at 0 and is assigned
    by the ring in write order.
*/
struct LogRecord
{
    std::int64_t timestampMs = 0;
    std::uint64_t sequence = 0;
    std::int32_t code = 0;
    LogLevel level = LogLevel::info;
    LogSource source = LogSource::general;
    char message[logMessageCapacity] = {};
};

static_assert (std::is_trivially_copyable<LogRecord>::value,
               "a log record must be copyable with memcpy: it is written on the audio thread");

/**
    A bounded, multi-producer, lock-free ring of structured log records.

    Why not a queue plus a mutex: the audio thread may record a health event
    (an overrun, a dropped MIDI message) without allocating, locking or doing
    file I/O. Every operation on the write path is a fixed number of atomic
    instructions and a bounded `memcpy`. Nothing blocks, and nothing can throw.

    The ring keeps the newest `capacity` records. When the producer outruns the
    consumer, older records are simply overwritten; `getOverwrittenCount()`
    reports how many. Readers copy the newest records into caller-provided
    storage, which is what the crash handler does with a static buffer.

    Threading:

    - `log()` may be called concurrently from any thread, including the audio
      thread.
    - `copyLatest()` may be called concurrently with writers. It only returns
      records whose generation marker is stable before and after the copy, so a
      record that is being overwritten is skipped rather than returned torn.
    - `clear()` is for tests and must not race with writers.

    A writer that is descheduled for longer than `capacity` intervening writes
    can have its slot reused. That is inherent to an overwriting ring and is
    acceptable for diagnostics.
*/
class LogRing final
{
public:
    explicit LogRing (std::size_t capacity = defaultLogCapacity);
    ~LogRing() = default;

    LogRing (const LogRing&) = delete;
    LogRing& operator= (const LogRing&) = delete;

    /** Appends one record. Returns its sequence number. Never allocates,
        never blocks, never throws. Safe on the audio thread. */
    std::uint64_t log (LogLevel level,
                       LogSource source,
                       std::int32_t code,
                       std::string_view message) noexcept;

    /** Copies up to `maxRecords` newest records into `destination`, oldest
        first. Returns how many were copied. Allocates nothing. */
    std::size_t copyLatest (LogRecord* destination, std::size_t maxRecords) const noexcept;

    /** Records accepted since construction / the last clear(). */
    std::uint64_t getPushedCount() const noexcept;

    /** Records overwritten before a reader could see them. */
    std::uint64_t getOverwrittenCount() const noexcept;

    std::size_t getCapacity() const noexcept { return capacity; }

    /** Clears the ring. Only for tests or while no writer is running. */
    void clear() noexcept;

private:
    struct Slot
    {
        /** 2 * generation while stable, 2 * generation + 1 while writing. */
        std::atomic<std::uint64_t> sequence { 0 };
        LogRecord record;
    };

    std::unique_ptr<Slot[]> slots;
    const std::size_t capacity;
    std::atomic<std::uint64_t> tail { 0 };
};

/** Process-wide ring used by the application. Created on first use, before
    any audio callback runs; it is never allocated from the audio thread. */
LogRing& defaultLog() noexcept;

/** Convenience: appends to `defaultLog()`. */
std::uint64_t log (LogLevel level,
                   LogSource source,
                   std::int32_t code,
                   std::string_view message) noexcept;

} // namespace ihub::diag
