#include "SandboxWatchdog.h"

#include "../SandboxHost.h"

#if JUCE_LINUX || JUCE_MAC
#include <sys/wait.h>
#endif

namespace ihub::plugins
{
    SandboxWatchdog::SandboxWatchdog (SandboxHost& hostToWatch, int timeoutMs)
        : juce::Thread ("ihub-sandbox-watchdog"), host (hostToWatch), hangTimeoutMs (timeoutMs)
    {
    }

    void SandboxWatchdog::run ()
    {
        auto* shared = host.shared;

        if (shared == nullptr)
            return;

        auto lastHeartbeat = sandbox::loadRelaxed (shared->heartbeat);
        auto lastProgressMs = juce::Time::getMillisecondCounterHiRes ();

        while (!threadShouldExit ())
        {
            wait (20);

            if (threadShouldExit () || host.shared == nullptr || host.childProcess == nullptr)
                return;

            (void)host.flushPublishedState ();

            int childStatus = 0;
            bool reaped = false;

#if JUCE_LINUX || JUCE_MAC
            const auto childPid = host.getChildProcessId ();

            if (childPid > 0)
            {
                const auto reapedPid = waitpid (childPid, &childStatus, WNOHANG);

                if (reapedPid == childPid)
                    reaped = true;
            }
#endif

            if (reaped || !host.childProcess->isRunning ())
            {
                juce::String reason = "sandbox process exited unexpectedly";

#if JUCE_LINUX || JUCE_MAC
                if (reaped)
                {
                    if (WIFSIGNALED (childStatus))
                        reason << " (signal " << WTERMSIG (childStatus) << ")";
                    else if (WIFEXITED (childStatus))
                        reason << " (exit " << WEXITSTATUS (childStatus) << ")";
                }
#endif

                host.markFailed (reason);
                return;
            }

            const auto now = juce::Time::getMillisecondCounterHiRes ();
            const auto heartbeat = sandbox::loadRelaxed (host.shared->heartbeat);

            if (heartbeat != lastHeartbeat)
            {
                lastHeartbeat = heartbeat;
                lastProgressMs = now;
                continue;
            }

            const auto outstanding = sandbox::loadAcquire (host.shared->requestPublished) -
                                     sandbox::loadAcquire (host.shared->requestConsumed);

            if (outstanding > 0 && now - lastProgressMs > static_cast<double> (hangTimeoutMs))
            {
                host.childProcess->kill ();
                host.markFailed ("sandbox stopped responding");
                return;
            }
        }
    }

}
