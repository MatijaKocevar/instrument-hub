#include "audio/WarmPool.h"

#include <chrono>
#include <exception>
#include <utility>

namespace AudioEngine
{

//==============================================================================
std::unique_ptr<WarmPool::Instance> WarmPool::createInstance (const Request& request,
                                                              juce::String& error)
{
    Factory currentFactory;
    bool shouldPrepare = false;
    double rate = 0.0;
    int blockSize = 0;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentFactory = factory;
        shouldPrepare = hasFormat;
        rate = sampleRate;
        blockSize = maximumBlockSize;
    }

    if (currentFactory == nullptr)
    {
        error = "no factory installed";
        return nullptr;
    }

    std::unique_ptr<Instance> created;

    try
    {
        created = currentFactory (request, error);
    }
    catch (const std::exception& exception)
    {
        error = juce::String ("factory threw: ") + exception.what();
        created.reset();
    }
    catch (...)
    {
        error = "factory threw an unknown exception";
        created.reset();
    }

    if (created != nullptr && shouldPrepare)
        created->prepare (rate, blockSize);

    return created;
}

void WarmPool::run()
{
    std::unique_lock<std::mutex> lock (mutex);

    for (;;)
    {
        if (quit && pendingDestroy.empty())
            break;

        // Retired instances are handed to the message thread before a new load,
        // so the live-instance count never exceeds the limit while a
        // replacement is created.
        if (! pendingDestroy.empty())
        {
            auto batch = std::move (pendingDestroy);
            pendingDestroy.clear();
            const auto hook = destroyHook;

            lock.unlock();

            for (auto& instance : batch)
                scheduleDestroy (std::move (instance), hook);

            batch.clear();

            lock.lock();

            condition.notify_all();
            continue;
        }

        if (quit)
            break;

        if (retryDeferred)
        {
            retryDeferred = false;
            requeueDeferredLocked();

            condition.notify_all();
            continue;
        }

        if (warmQueue.empty())
        {
            // A slot deferred earlier becomes reachable again as soon as a pin
            // is dropped or a delete completes.
            if ((liveCountLocked() < limitLocked()
                 || pendingDeletes->load (std::memory_order_relaxed) > 0)
                && requeueDeferredLocked())
            {
                condition.notify_all();
                continue;
            }

            condition.notify_all();
            condition.wait (lock);
            continue;
        }

        const int slotIndex = warmQueue.front();
        warmQueue.erase (warmQueue.begin());
        slots[static_cast<size_t> (slotIndex)].queued = false;

        if (! needsWarmLocked (slotIndex))
        {
            condition.notify_all();
            continue;
        }

        if (liveCountLocked() >= limitLocked())
        {
            if (evictOneLocked())
            {
                // Re-queue and let the retire pass run first, so the count
                // really drops before this load starts.
                warmQueue.insert (warmQueue.begin(), slotIndex);
                slots[static_cast<size_t> (slotIndex)].queued = true;
                continue;
            }

            if (pendingDeletes->load (std::memory_order_relaxed) > 0)
            {
                // Room is on its way: the message thread just has to run the
                // posted deletes. Poll briefly rather than spin.
                warmQueue.insert (warmQueue.begin(), slotIndex);
                slots[static_cast<size_t> (slotIndex)].queued = true;
                condition.wait_for (lock, std::chrono::milliseconds (2));
                continue;
            }

            // Every other instance is pinned; try again after a release().
            slots[static_cast<size_t> (slotIndex)].deferred = true;
            condition.notify_all();
            continue;
        }

        const Request request = slots[static_cast<size_t> (slotIndex)].request;

        warming = true;
        loadingSlot = slotIndex;
        condition.notify_all();
        lock.unlock();

        juce::String error;
        auto created = createInstance (request, error);

        lock.lock();
        warming = false;
        loadingSlot = -1;

        auto& slot = slots[static_cast<size_t> (slotIndex)];
        const bool stillNeeded = needsWarmLocked (slotIndex);
        bool becameWarm = false;

        if (created != nullptr && stillNeeded && liveCountLocked() < limitLocked())
        {
            slot.instance = std::move (created);
            slot.lastUsed = ++useCounter;
            slot.lastError.clear();
            slot.failedId.clear();
            ++warmCount;
            becameWarm = true;
        }
        else
        {
            if (created != nullptr)
                pendingDestroy.push_back (std::move (created));

            if (error.isNotEmpty())
            {
                slot.lastError = error;
                slot.failedId = request.id;
            }
            else if (stillNeeded && isProtectedLocked (slotIndex))
            {
                // A message-thread eviction filled the budget while this load
                // was in flight. Try again once it has been freed.
                slot.deferred = true;
            }
        }

        condition.notify_all();

        if (becameWarm)
        {
            // The callback binds the instance and may finish an arm that was
            // waiting for this cold patch. It runs without the pool lock so it
            // can call acquire()/release().
            auto callback = warmCallback;
            lock.unlock();

            if (callback != nullptr)
            {
                try
                {
                    callback (slotIndex);
                }
                catch (...)
                {
                }
            }

            lock.lock();
        }
    }

    condition.notify_all();
}

void WarmPool::notifyWorker()
{
    condition.notify_all();
}

} // namespace AudioEngine
