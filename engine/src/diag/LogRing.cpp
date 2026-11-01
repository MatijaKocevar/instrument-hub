#include "LogRing.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace ihub::diag
{

namespace
{
std::int64_t currentTimeMilliseconds() noexcept
{
    return static_cast<std::int64_t> (
        std::chrono::duration_cast<std::chrono::milliseconds> (
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void copyMessage (char* destination, std::string_view message) noexcept
{
    const auto length = std::min (message.size(), logMessageCapacity - 1);

    if (length > 0)
        std::memcpy (destination, message.data(), length);

    destination[length] = '\0';
}

char lowerAscii (char value) noexcept
{
    return (value >= 'A' && value <= 'Z') ? static_cast<char> (value - 'A' + 'a')
                                          : value;
}

bool equalsIgnoreCase (std::string_view text, const char* literal) noexcept
{
    const auto length = std::strlen (literal);

    if (text.size() != length)
        return false;

    for (std::size_t index = 0; index < length; ++index)
        if (lowerAscii (text[index]) != lowerAscii (literal[index]))
            return false;

    return true;
}

}

const char* toString (LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::trace:   return "TRACE";
        case LogLevel::debug:   return "DEBUG";
        case LogLevel::info:    return "INFO";
        case LogLevel::warning: return "WARN";
        case LogLevel::error:   return "ERROR";
        case LogLevel::fatal:   return "FATAL";
    }

    return "INFO";
}

const char* toString (LogSource source) noexcept
{
    switch (source)
    {
        case LogSource::general:     return "general";
        case LogSource::audio:       return "audio";
        case LogSource::midi:        return "midi";
        case LogSource::controller:  return "controller";
        case LogSource::api:         return "api";
        case LogSource::plugins:     return "plugins";
        case LogSource::project:     return "project";
        case LogSource::sampler:     return "sampler";
        case LogSource::ui:          return "ui";
        case LogSource::diagnostics: return "diagnostics";
    }

    return "general";
}

bool parseLogLevel (std::string_view text, LogLevel& level) noexcept
{
    struct Entry
    {
        const char* name;
        LogLevel level;
    };

    static constexpr Entry entries[] = {
        { "trace", LogLevel::trace }, { "debug", LogLevel::debug },
        { "info", LogLevel::info },   { "warn", LogLevel::warning },
        { "warning", LogLevel::warning }, { "error", LogLevel::error },
        { "fatal", LogLevel::fatal }
    };

    for (const auto& entry : entries)
    {
        if (equalsIgnoreCase (text, entry.name))
        {
            level = entry.level;
            return true;
        }
    }

    return false;
}

bool parseLogSource (std::string_view text, LogSource& source) noexcept
{
    struct Entry
    {
        const char* name;
        LogSource source;
    };

    static constexpr Entry entries[] = {
        { "general", LogSource::general }, { "audio", LogSource::audio },
        { "midi", LogSource::midi },       { "controller", LogSource::controller },
        { "api", LogSource::api },         { "plugins", LogSource::plugins },
        { "project", LogSource::project }, { "sampler", LogSource::sampler },
        { "ui", LogSource::ui },           { "diagnostics", LogSource::diagnostics }
    };

    for (const auto& entry : entries)
    {
        if (equalsIgnoreCase (text, entry.name))
        {
            source = entry.source;
            return true;
        }
    }

    return false;
}

LogRing::LogRing (std::size_t capacityToUse)
    : slots (new Slot[std::max<std::size_t> (1, capacityToUse)]),
      capacity (std::max<std::size_t> (1, capacityToUse))
{
}

std::uint64_t LogRing::log (LogLevel level,
                            LogSource source,
                            std::int32_t code,
                            std::string_view message) noexcept
{
    const auto sequence = tail.fetch_add (1, std::memory_order_acq_rel);

    auto& slot = slots[sequence % capacity];

    slot.sequence.store (sequence * 2 + 1, std::memory_order_release);

    auto& record = slot.record;
    record.timestampMs = currentTimeMilliseconds();
    record.sequence = sequence;
    record.code = code;
    record.level = level;
    record.source = source;
    copyMessage (record.message, message);

    slot.sequence.store (sequence * 2 + 2, std::memory_order_release);

    return sequence;
}

std::size_t LogRing::copyLatest (LogRecord* destination, std::size_t maxRecords) const noexcept
{
    if (destination == nullptr || maxRecords == 0)
        return 0;

    const auto end = tail.load (std::memory_order_acquire);
    auto start = end > capacity ? end - capacity : std::uint64_t { 0 };

    if (end - start > maxRecords)
        start = end - static_cast<std::uint64_t> (maxRecords);

    std::size_t written = 0;

    for (auto sequence = start; sequence < end && written < maxRecords; ++sequence)
    {
        auto& slot = slots[sequence % capacity];
        const auto marker = slot.sequence.load (std::memory_order_acquire);

        if (marker != sequence * 2 + 2)
            continue;

        LogRecord copy = slot.record;

        if (slot.sequence.load (std::memory_order_acquire) != marker)
            continue;

        destination[written++] = copy;
    }

    return written;
}

std::uint64_t LogRing::getPushedCount() const noexcept
{
    return tail.load (std::memory_order_acquire);
}

std::uint64_t LogRing::getOverwrittenCount() const noexcept
{
    const auto pushed = getPushedCount();

    return pushed > capacity ? pushed - capacity : 0;
}

void LogRing::clear() noexcept
{
    for (std::size_t index = 0; index < capacity; ++index)
    {
        slots[index].sequence.store (0, std::memory_order_relaxed);
        slots[index].record = LogRecord {};
    }

    tail.store (0, std::memory_order_release);
}

LogRing& defaultLog() noexcept
{
    static LogRing ring;
    return ring;
}

std::uint64_t log (LogLevel level,
                   LogSource source,
                   std::int32_t code,
                   std::string_view message) noexcept
{
    return defaultLog().log (level, source, code, message);
}

}
