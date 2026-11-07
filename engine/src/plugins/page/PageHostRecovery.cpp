#include "../PageHost.h"

#include <chrono>

namespace ihub::plugins
{
namespace
{
constexpr int watchdogIntervalMs = 20;
}

void PageHost::requestRecovery (const juce::String& reason, bool manual)
{
    juce::ignoreUnused (manual);

    {
        const std::lock_guard<std::mutex> lock (recoveryMutex);

        if (shuttingDown)
            return;

        if (recoveryRequested)
            return;

        if (status.load (std::memory_order_acquire) == Status::recovering)
            return;

        recoveryRequested = true;
    }

    markStatus (Status::recovering, reason);
    recoveryWake.notify_one();
}

bool PageHost::requestRestart()
{
    requestRecovery ("manual plugin page restart requested", true);
    return true;
}

bool PageHost::restartAndWait (int timeoutMs, juce::String& error)
{
    error.clear();
    requestRestart();

    const auto deadline = juce::Time::getMillisecondCounterHiRes()
                        + static_cast<double> (juce::jmax (1, timeoutMs));

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        const auto current = status.load (std::memory_order_acquire);

        if (current == Status::ready)
            return true;

        if (current == Status::failed)
        {
            error = getFailureReason();
            return false;
        }

        juce::Thread::sleep (10);
    }

    error = "the shared plugin page did not recover in time";
    return false;
}

void PageHost::watchdogLoop()
{
    std::vector<std::uint32_t> lastHeartbeat (slots.size(), 0);
    std::vector<std::uint64_t> lastOutstanding (slots.size(), 0);
    std::vector<double> lastProgress (slots.size(),
                                      juce::Time::getMillisecondCounterHiRes());

    while (true)
    {
        {
            const std::lock_guard<std::mutex> lock (recoveryMutex);

            if (shuttingDown)
                return;
        }

        juce::Thread::sleep (watchdogIntervalMs);

        {
            const std::lock_guard<std::mutex> lock (recoveryMutex);

            if (shuttingDown)
                return;
        }

        if (status.load (std::memory_order_acquire) != Status::ready)
            continue;

        flushAllPublishedStates();

        if (childProcess == nullptr || ! childProcess->isRunning())
        {
            juce::String reason = "shared plugin host exited unexpectedly";

            {
                const std::lock_guard<std::mutex> lock (outputMutex);
                const auto tail = outputTail.trim();

                if (tail.isNotEmpty())
                {
                    reason << ": "
                           << (tail.length() > 4000
                                   ? tail.substring (tail.length() - 4000)
                                   : tail);

                    const auto log = juce::File::getSpecialLocation (
                                         juce::File::tempDirectory)
                                         .getChildFile ("ihub-page-host-crash.log");
                    log.appendText ("--- "
                                    + juce::Time::getCurrentTime().toString (true, true)
                                    + " ---\n" + tail + "\n");
                }
            }

            requestRecovery (reason, false);
            continue;
        }

        auto* page = sharedPage.load (std::memory_order_acquire);

        if (page == nullptr)
        {
            requestRecovery ("shared plugin page is not mapped", false);
            continue;
        }

        const auto now = juce::Time::getMillisecondCounterHiRes();

        for (std::size_t index = 0; index < slots.size(); ++index)
        {
            auto& shared = page->slots[index];
            const auto heartbeat = sandbox::loadRelaxed (shared.heartbeat);
            const auto outstanding = sandbox::loadAcquire (shared.requestPublished)
                                   - sandbox::loadAcquire (shared.requestConsumed);

            if (heartbeat != lastHeartbeat[index])
            {
                lastHeartbeat[index] = heartbeat;
                lastOutstanding[index] = outstanding;
                lastProgress[index] = now;
                continue;
            }

            if (slots[index]->controlInFlight.load (std::memory_order_relaxed) > 0)
            {
                lastOutstanding[index] = outstanding;
                lastProgress[index] = now;
                continue;
            }

            if (outstanding == 0)
            {
                lastOutstanding[index] = 0;
                lastProgress[index] = now;
                continue;
            }

            if (lastOutstanding[index] == 0)
            {
                lastOutstanding[index] = outstanding;
                lastProgress[index] = now;
                continue;
            }

            lastOutstanding[index] = outstanding;

            if (now - lastProgress[index] > static_cast<double> (options.hangTimeoutMs))
            {
                requestRecovery ("shared plugin host stopped responding (slot "
                                     + juce::String (slots[index]->descriptor.slotIndex) + ")",
                                 false);
                return;
            }
        }
    }
}

void PageHost::recoveryLoop()
{
    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock (recoveryMutex);

            recoveryWake.wait (lock, [this]
            {
                return shuttingDown || recoveryRequested;
            });

            if (shuttingDown)
                return;

            recoveryRequested = false;
        }

        markStatus (Status::recovering, {});
        runRecovery();
    }
}

void PageHost::captureHealthyStates() noexcept
{
    auto* page = sharedPage.load (std::memory_order_acquire);

    if (page == nullptr)
        return;

    for (auto& slot : slots)
    {
        if (childProcess == nullptr || ! childProcess->isRunning())
            return;

        juce::MemoryBlock output;
        juce::MemoryBlock ignored;

        if (runControlCommand (*slot, sandbox::ControlCommand::captureState,
                               ignored, output, options.recoveryCaptureTimeoutMs)
            && output.getSize() > 0)
        {
            commitState (*slot, output, true);
        }
    }
}

bool PageHost::applyCommittedStates (juce::String& error)
{
    for (auto& slot : slots)
    {
        {
            const std::lock_guard<std::mutex> lock (slot->stateLock);
            slot->localError.clear();
        }

        slot->failed.store (false, std::memory_order_relaxed);
        slot->underruns.store (0, std::memory_order_relaxed);
        slot->droppedRequests.store (0, std::memory_order_relaxed);
        slot->droppedMidiEvents.store (0, std::memory_order_relaxed);
        slot->playedBlocks.store (0, std::memory_order_relaxed);
        slot->repliesPlayed = 0;
        slot->seenEditorStateRevision = 0;

        juce::MemoryBlock state;

        {
            const std::lock_guard<std::mutex> lock (slot->stateLock);
            state = slot->lastCommittedState;
        }

        if (state.getSize() == 0)
            continue;

        juce::MemoryBlock ignored;

        if (! runControlCommand (*slot, sandbox::ControlCommand::applyState,
                                 state, ignored, options.recoveryStateTimeoutMs))
        {
            const auto reason = "could not restore slot "
                              + juce::String (slot->descriptor.slotIndex);

            {
                const std::lock_guard<std::mutex> lock (slot->stateLock);
                slot->localError = reason;
                slot->lastCommittedState.reset();
                slot->lastCommittedRevision = 0;
            }

            slot->failed.store (true, std::memory_order_relaxed);
            error = reason;
            continue;
        }
    }

    return true;
}

void PageHost::runRecovery()
{
    captureHealthyStates();

    juce::String lastError;

    for (int attempt = 0; attempt < options.recoveryAttempts; ++attempt)
    {
        {
            const std::lock_guard<std::mutex> lock (recoveryMutex);

            if (shuttingDown)
                return;
        }

        if (attempt > 0)
        {
            const auto delay = options.recoveryBackoffMs * (1 << (attempt - 1));

            for (int elapsed = 0; elapsed < delay; elapsed += 20)
            {
                juce::Thread::sleep (20);

                const std::lock_guard<std::mutex> lock (recoveryMutex);

                if (shuttingDown)
                    return;
            }
        }

        juce::String error;
        bool started = false;

        {
            const std::lock_guard<std::mutex> lock (lifecycleMutex);
            stopProcess (true);
            started = startProcess (error);
        }

        if (! started)
        {
            lastError = error;
            markStatus (Status::recovering, error);
            continue;
        }

        if (! applyCommittedStates (error))
        {
            lastError = error;
            markStatus (Status::recovering, error);

            const std::lock_guard<std::mutex> lock (lifecycleMutex);
            stopProcess (true);
            continue;
        }

        markStatus (Status::ready, {});

        RecoveredCallback callback;

        {
            const std::lock_guard<std::mutex> lock (statusLock);
            callback = recoveredCallback;
        }

        if (callback != nullptr)
        {
            try
            {
                callback (activeSlot.load (std::memory_order_relaxed));
            }
            catch (...)
            {
            }
        }

        return;
    }

    if (lastError.isEmpty())
        lastError = "the shared plugin host could not be restarted";

    markStatus (Status::failed, lastError);
}

} // namespace ihub::plugins
