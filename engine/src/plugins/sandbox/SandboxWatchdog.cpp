#include "SandboxWatchdog.h"

#include "../SandboxHost.h"

namespace ihub::plugins
{
SandboxWatchdog::SandboxWatchdog (SandboxHost& hostToWatch, int timeoutMs)
    : juce::Thread ("ihub-sandbox-watchdog"),
      host (hostToWatch),
      hangTimeoutMs (timeoutMs)
{
}

void SandboxWatchdog::run()
{
    auto* shared = host.shared;

    if (shared == nullptr)
        return;

    auto lastHeartbeat = sandbox::loadRelaxed (shared->heartbeat);
    auto lastProgressMs = juce::Time::getMillisecondCounterHiRes();

    while (! threadShouldExit())
    {
        wait (20);

        if (threadShouldExit() || host.shared == nullptr || host.childProcess == nullptr)
            return;

        if (! host.childProcess->isRunning())
        {
            host.markFailed ("sandbox process exited unexpectedly");
            return;
        }

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto heartbeat = sandbox::loadRelaxed (host.shared->heartbeat);

        if (heartbeat != lastHeartbeat)
        {
            lastHeartbeat = heartbeat;
            lastProgressMs = now;
            continue;
        }

        const auto outstanding = sandbox::loadAcquire (host.shared->requestPublished)
                               - sandbox::loadAcquire (host.shared->requestConsumed);

        if (outstanding > 0 && now - lastProgressMs > static_cast<double> (hangTimeoutMs))
        {
            host.childProcess->kill();
            host.markFailed ("sandbox stopped responding");
            return;
        }
    }
}

}
