#pragma once

#include "LogRing.h"

#include <cstdint>
#include <string>

namespace ihub::diag
{

    inline constexpr std::uint32_t crashDumpFormatVersion = 1;

    struct CrashDumpOptions
    {
        std::string path;

        LogRing* ring = nullptr;

        bool dumpOnTermination = false;

        bool reRaiseSignal = true;
    };

    bool installCrashDump (const CrashDumpOptions& options = {});

    void uninstallCrashDump () noexcept;

    bool isCrashDumpInstalled () noexcept;

    std::string getCrashDumpPath ();

    std::string defaultCrashDumpPath ();

    bool writeCrashDump (int fd, const LogRing& ring, int signalNumber) noexcept;

}
