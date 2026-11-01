#include "../SandboxHost.h"

#include "SandboxWatchdog.h"

namespace ihub::plugins
{
void SandboxHost::closeSharedMemory() noexcept
{
    mapping.reset();
    shared = nullptr;

    if (sharedMemoryFile != juce::File())
    {
        sharedMemoryFile.deleteFile();
        sharedMemoryFile = juce::File();
    }
}

void SandboxHost::stop()
{
    if (watchdog != nullptr)
    {
        watchdog->signalThreadShouldExit();
        watchdog->notify();
        watchdog->waitForThreadToExit (2000);
        watchdog.reset();
    }

    if (shared != nullptr)
        sandbox::storeRelease (shared->state,
                               static_cast<std::int32_t> (sandbox::ProcessState::stopping));

    if (childProcess != nullptr)
    {
        if (childProcess->isRunning())
        {
            childProcess->waitForProcessToFinish (300);

            if (childProcess->isRunning())
            {
                childProcess->kill();
                childProcess->waitForProcessToFinish (1000);
            }
        }

        childProcess->readAllProcessOutput();
        childProcess.reset();
    }

    closeSharedMemory();

    failed.store (false, std::memory_order_relaxed);
    status.store (Status::stopped, std::memory_order_relaxed);
}

} // namespace ihub::plugins
