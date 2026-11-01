#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>

namespace ihub::diag
{

enum class LogLevel : std::uint8_t
{
    trace = 0,
    debug,
    info,
    warning,
    error,
    fatal
};

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

class LogRing final
{
public:
    explicit LogRing (std::size_t capacity = defaultLogCapacity);
    ~LogRing() = default;

    LogRing (const LogRing&) = delete;
    LogRing& operator= (const LogRing&) = delete;

    std::uint64_t log (LogLevel level,
                       LogSource source,
                       std::int32_t code,
                       std::string_view message) noexcept;

    std::size_t copyLatest (LogRecord* destination, std::size_t maxRecords) const noexcept;

    std::uint64_t getPushedCount() const noexcept;

    std::uint64_t getOverwrittenCount() const noexcept;

    std::size_t getCapacity() const noexcept { return capacity; }

    void clear() noexcept;

private:
    struct Slot
    {
        std::atomic<std::uint64_t> sequence { 0 };
        LogRecord record;
    };

    std::unique_ptr<Slot[]> slots;
    const std::size_t capacity;
    std::atomic<std::uint64_t> tail { 0 };
};

LogRing& defaultLog() noexcept;

std::uint64_t log (LogLevel level,
                   LogSource source,
                   std::int32_t code,
                   std::string_view message) noexcept;

}
