#include "CrashDump.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
    #include <cstdio>
#else
    #include <ctime>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace ihub::diag
{

namespace
{
constexpr std::size_t maxDumpPath = 512;
constexpr std::size_t maxDumpRecords = 1024;

char dumpPath[maxDumpPath] = {};
volatile sig_atomic_t dumpInstalled = 0;
volatile sig_atomic_t dumpInProgress = 0;
volatile sig_atomic_t dumpReRaise = 1;
LogRing* dumpRing = nullptr;

LogRecord dumpRecords[maxDumpRecords];

#if ! defined(_WIN32)
constexpr int maxHandledSignals = 7;

const int handledSignals[maxHandledSignals] = {
    SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL, SIGTERM, SIGINT
};

struct sigaction previousActions[maxHandledSignals] = {};
int installedSignalCount = 0;

alignas(16) char alternateStack[64 * 1024];
stack_t previousAlternateStack = {};
bool alternateStackInstalled = false;

const char* signalName (int signalNumber) noexcept
{
    switch (signalNumber)
    {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGTERM: return "SIGTERM";
        case SIGINT:  return "SIGINT";
        default:      return "UNKNOWN";
    }
}

char* appendText (char* out, char* end, const char* text) noexcept
{
    while (text != nullptr && *text != '\0' && out < end)
        *out++ = *text++;

    return out;
}

char* appendUnsigned (char* out, char* end, std::uint64_t value) noexcept
{
    char digits[24];
    int count = 0;

    do
    {
        digits[count++] = static_cast<char> ('0' + value % 10);
        value /= 10;
    }
    while (value != 0);

    while (count > 0 && out < end)
        *out++ = digits[--count];

    return out;
}

char* appendSigned (char* out, char* end, std::int64_t value) noexcept
{
    if (value < 0)
    {
        if (out < end)
            *out++ = '-';

        return appendUnsigned (out, end, static_cast<std::uint64_t> (-(value + 1)) + 1);
    }

    return appendUnsigned (out, end, static_cast<std::uint64_t> (value));
}

bool writeAll (int fd, const char* data, std::size_t length) noexcept
{
    while (length > 0)
    {
        const auto written = ::write (fd, data, length);

        if (written < 0)
        {
            if (errno == EINTR)
                continue;

            return false;
        }

        data += written;
        length -= static_cast<std::size_t> (written);
    }

    return true;
}

std::int64_t realtimeMilliseconds() noexcept
{
    timespec now {};
    clock_gettime (CLOCK_REALTIME, &now);

    return static_cast<std::int64_t> (now.tv_sec) * 1000
           + static_cast<std::int64_t> (now.tv_nsec / 1000000);
}

void crashSignalHandler (int signalNumber)
{
    if (dumpInProgress != 0)
        _exit (128 + signalNumber);

    dumpInProgress = 1;

    if (dumpRing != nullptr)
    {
        const int fd = ::open (dumpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd >= 0)
        {
            writeCrashDump (fd, *dumpRing, signalNumber);
            ::fsync (fd);
            ::close (fd);
        }
    }

    if (dumpReRaise != 0)
    {
        struct sigaction defaultAction {};
        defaultAction.sa_handler = SIG_DFL;
        sigemptyset (&defaultAction.sa_mask);
        sigaction (signalNumber, &defaultAction, nullptr);

        sigset_t unblocked;
        sigemptyset (&unblocked);
        sigaddset (&unblocked, signalNumber);
        sigprocmask (SIG_UNBLOCK, &unblocked, nullptr);

        ::raise (signalNumber);
    }

    _exit (128 + signalNumber);
}
#endif
}

bool writeCrashDump (int fd, const LogRing& ring, int signalNumber) noexcept
{
#if defined(_WIN32)
    (void) fd;
    (void) ring;
    (void) signalNumber;
    return false;
#else
    const auto count = ring.copyLatest (dumpRecords, maxDumpRecords);

    char line[512];
    char* const lineEnd = line + sizeof (line);
    char* out = line;

    auto flush = [fd, &line, &out]
    {
        const bool ok = writeAll (fd, line, static_cast<std::size_t> (out - line));
        out = line;
        return ok;
    };

    out = appendText (out, lineEnd, "instrument-hub crash dump v");
    out = appendUnsigned (out, lineEnd, crashDumpFormatVersion);
    out = appendText (out, lineEnd, "\n");
    out = appendText (out, lineEnd, "format: ");
    out = appendUnsigned (out, lineEnd, crashDumpFormatVersion);
    out = appendText (out, lineEnd, "\n");
    out = appendText (out, lineEnd, "signal: ");
    out = appendUnsigned (out, lineEnd, static_cast<std::uint64_t> (signalNumber));
    out = appendText (out, lineEnd, " ");
    out = appendText (out, lineEnd, signalName (signalNumber));
    out = appendText (out, lineEnd, "\n");
    out = appendText (out, lineEnd, "time_ms: ");
    out = appendSigned (out, lineEnd, realtimeMilliseconds());
    out = appendText (out, lineEnd, "\n");
    out = appendText (out, lineEnd, "records: ");
    out = appendUnsigned (out, lineEnd, static_cast<std::uint64_t> (count));
    out = appendText (out, lineEnd, "\n");
    out = appendText (out, lineEnd, "---\n");

    if (! flush())
        return false;

    for (std::size_t index = 0; index < count; ++index)
    {
        const auto& record = dumpRecords[index];

        out = appendSigned (out, lineEnd, record.timestampMs);
        out = appendText (out, lineEnd, " ");
        out = appendText (out, lineEnd, toString (record.level));
        out = appendText (out, lineEnd, " ");
        out = appendText (out, lineEnd, toString (record.source));
        out = appendText (out, lineEnd, " ");
        out = appendSigned (out, lineEnd, record.code);
        out = appendText (out, lineEnd, " ");

        for (std::size_t c = 0; c < logMessageCapacity && record.message[c] != '\0'; ++c)
        {
            if (out >= lineEnd)
            {
                if (! flush())
                    return false;
            }

            *out++ = record.message[c];
        }

        out = appendText (out, lineEnd, "\n");

        if (! flush())
            return false;
    }

    out = appendText (out, lineEnd, "end\n");
    return flush();
#endif
}

bool installCrashDump (const CrashDumpOptions& options)
{
#if defined(_WIN32)
    (void) options;
    return false;
#else
    uninstallCrashDump();

    const auto path = options.path.empty() ? defaultCrashDumpPath() : options.path;

    const auto length = std::min (path.size(), maxDumpPath - 1);
    std::memcpy (dumpPath, path.data(), length);
    dumpPath[length] = '\0';

    dumpRing = options.ring != nullptr ? options.ring : &defaultLog();
    dumpReRaise = options.reRaiseSignal ? 1 : 0;
    dumpInProgress = 0;

    if (! alternateStackInstalled)
    {
        stack_t stack {};
        stack.ss_sp = alternateStack;
        stack.ss_size = sizeof (alternateStack);
        stack.ss_flags = 0;

        if (sigaltstack (&stack, &previousAlternateStack) == 0)
            alternateStackInstalled = true;
    }

    struct sigaction action {};
    action.sa_handler = crashSignalHandler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_ONSTACK;

    installedSignalCount = 0;
    const int signalCount = options.dumpOnTermination ? maxHandledSignals
                                                      : maxHandledSignals - 2;

    for (int index = 0; index < signalCount; ++index)
    {
        if (sigaction (handledSignals[index],
                       &action,
                       &previousActions[index]) != 0)
        {
            for (int restore = 0; restore < installedSignalCount; ++restore)
                sigaction (handledSignals[restore], &previousActions[restore], nullptr);

            installedSignalCount = 0;
            dumpRing = nullptr;
            return false;
        }

        ++installedSignalCount;
    }

    dumpInstalled = 1;
    return true;
#endif
}

void uninstallCrashDump() noexcept
{
#if ! defined(_WIN32)
    for (int index = 0; index < installedSignalCount; ++index)
        sigaction (handledSignals[index], &previousActions[index], nullptr);

    installedSignalCount = 0;

    if (alternateStackInstalled)
    {
        sigaltstack (&previousAlternateStack, nullptr);
        alternateStackInstalled = false;
    }

    dumpInstalled = 0;
    dumpRing = nullptr;
#endif
}

bool isCrashDumpInstalled() noexcept
{
    return dumpInstalled != 0;
}

std::string defaultCrashDumpPath()
{
#if defined(_WIN32)
    return "instrument-hub-crash.log";
#else
    const char* temp = std::getenv ("TMPDIR");

    std::string directory = (temp != nullptr && temp[0] != '\0') ? temp : "/tmp";

    if (! directory.empty() && directory.back() != '/')
        directory += '/';

    return directory + "instrument-hub-crash.log";
#endif
}

std::string getCrashDumpPath()
{
    if (dumpPath[0] != '\0')
        return std::string (dumpPath);

    return defaultCrashDumpPath();
}

}
