#include "SignalHandler.h"
#include "TestSupport.h"

#include "diag/CrashDump.h"
#include "diag/LogRing.h"

#include <csignal>
#include <iostream>

#if ! defined(_WIN32)
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace DiagTest
{

void runSignalHandler()
{
#if ! defined(_WIN32)
    std::cout << "\n-- crash handler --" << std::endl;

    const auto file = tempFile (".log");
    const auto path = file.getFullPathName().toStdString();
    file.deleteFile();

    const pid_t pid = ::fork();

    if (pid == 0)
    {
        ihub::diag::LogRing ring (8);
        ring.log (ihub::diag::LogLevel::info,
                  ihub::diag::LogSource::diagnostics,
                  1,
                  "before the crash");

        ihub::diag::CrashDumpOptions options;
        options.path = path;
        options.ring = &ring;

        if (! ihub::diag::installCrashDump (options))
            ::_exit (97);

        ring.log (ihub::diag::LogLevel::info,
                  ihub::diag::LogSource::diagnostics,
                  2,
                  "handler installed");

        ::raise (SIGABRT);
        ::_exit (98);
    }

    int status = 0;
    const auto waited = ::waitpid (pid, &status, 0);

    check (waited == pid, "the child was reaped");

    const bool signalled = WIFSIGNALED (status);
    check (signalled && WTERMSIG (status) == SIGABRT,
           "the original SIGABRT status is preserved");

    const auto content = file.loadFileAsString().toStdString();

    check (contains (content, "SIGABRT"), "the handler recorded SIGABRT");
    check (contains (content, "before the crash"),
           "records written before the handler was installed are dumped");
    check (contains (content, "handler installed"),
           "records written after installation are dumped");

    file.deleteFile();
#endif
}

}
