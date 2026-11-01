#pragma once

#include <juce_core/juce_core.h>

namespace ihub::plugins
{
class SandboxHost;

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

}
