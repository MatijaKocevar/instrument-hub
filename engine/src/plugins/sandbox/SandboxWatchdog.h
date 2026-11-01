#pragma once

#include <juce_core/juce_core.h>

namespace ihub::plugins
{
class SandboxHost;

/** Polls the child for death and for a plugin that has stopped answering.

    `isRunning()` is a `waitpid`, so it cannot run on the audio thread. This
    thread owns that check; `process()` only reads the atomics the watchdog and
    the client publish. */
class SandboxWatchdog final : public juce::Thread
{
public:
    SandboxWatchdog (SandboxHost& hostToWatch, int timeoutMs);

    void run() override;

private:
    SandboxHost& host;
    const int hangTimeoutMs;

    JUCE_DECLARE_NON_COPYABLE (SandboxWatchdog)
};

} // namespace ihub::plugins
