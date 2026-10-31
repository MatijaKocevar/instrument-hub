#pragma once

#include "LogRing.h"

#include <cstdint>
#include <string>

namespace ihub::diag
{

inline constexpr std::uint32_t crashDumpFormatVersion = 1;

struct CrashDumpOptions
{
    /** Destination file. Empty means `defaultCrashDumpPath()`. */
    std::string path;

    /** The ring dumped when a signal arrives. Null means `defaultLog()`. */
    LogRing* ring = nullptr;

    /** Also treat SIGTERM and SIGINT as a dump trigger. Off by default: a
        requested quit is not a crash. */
    bool dumpOnTermination = false;

    /** Re-raise the original signal after writing the dump, preserving the
        exit status and any core dump. On by default. */
    bool reRaiseSignal = true;
};

/** Installs signal handlers that write the log ring to a file on a crash.

    Handled signals: SIGSEGV, SIGBUS, SIGABRT, SIGFPE and SIGILL. The handler
    runs on a dedicated alternate signal stack so a stack overflow can still be
    reported, and uses only async-signal-safe calls (`open`, `write`, `fsync`,
    `close`, `raise`). It writes the header first, then every valid record
    oldest-first, flushes the file, and re-raises the signal so the process
    dies with the original status.

    Installing twice replaces the previous installation. Returns false only
    when the platform is not POSIX or the signal handlers could not be
    installed.
*/
bool installCrashDump (const CrashDumpOptions& options = {});

/** Restores the previous signal handlers. */
void uninstallCrashDump() noexcept;

bool isCrashDumpInstalled() noexcept;

/** The path currently used by the installed handler, or the default path when
    none is installed. */
std::string getCrashDumpPath();

/** `$TMPDIR/instrument-hub-crash.log`, or `/tmp/instrument-hub-crash.log`. */
std::string defaultCrashDumpPath();

/** Writes a complete dump for `signalNumber` to `fd`.

    This is the exact code path the signal handler uses, exposed so tests can
    verify the format without forking. It allocates nothing and uses only
    async-signal-safe calls. Returns false when a write failed.
*/
bool writeCrashDump (int fd, const LogRing& ring, int signalNumber) noexcept;

} // namespace ihub::diag
