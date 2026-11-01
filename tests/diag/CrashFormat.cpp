#include "CrashFormat.h"
#include "TestSupport.h"

#include "diag/CrashDump.h"
#include "diag/LogRing.h"

#include <csignal>
#include <iostream>

#if ! defined(_WIN32)
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace DiagTest
{

void runCrashDumpFormat()
{
#if ! defined(_WIN32)
    std::cout << "\n-- crash dump format --" << std::endl;

    const auto file = tempFile (".log");
    const auto path = file.getFullPathName().toStdString();

    ihub::diag::LogRing ring (8);
    ring.log (ihub::diag::LogLevel::info,
              ihub::diag::LogSource::audio,
              1,
              "device opened");
    ring.log (ihub::diag::LogLevel::fatal,
              ihub::diag::LogSource::general,
              99,
              "about to fall over");

    const int fd = ::open (path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    check (fd >= 0, "the dump file opens");

    const bool written = fd >= 0
                         && ihub::diag::writeCrashDump (fd, ring, SIGABRT);
    if (fd >= 0)
        ::close (fd);

    check (written, "writeCrashDump reports success");

    const auto content = file.loadFileAsString().toStdString();

    check (contains (content, "instrument-hub crash dump v1"),
           "the dump starts with the version header");
    check (contains (content, "signal: 6 SIGABRT"), "the signal is recorded");
    check (contains (content, "records: 2"), "the record count is recorded");
    check (contains (content, "device opened"), "the first record is present");
    check (contains (content, "about to fall over"), "the last record is present");
    check (contains (content, "FATAL general 99 about to fall over"),
           "records carry level, source and structured code");
    check (contains (content, "\nend\n"), "the dump has a terminator");

    file.deleteFile();
#endif
}

}
