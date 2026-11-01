#include "session/WarmPoolController.h"

#include "audio/SlotSwitcher.h"

namespace ihub::session
{

//==============================================================================
void WarmPoolController::arm (int slot)
{
    if (switcher == nullptr)
        return;

    if (slot < 0)
    {
        disarm();
        return;
    }

    const std::lock_guard<std::mutex> lock (armMutex);

    // A cold slot reports `warming` from this store until the instance is
    // resident. A warm slot completes below without ever publishing it.
    pendingArmSlot.store (slot, std::memory_order_relaxed);
    requestedSlot.store (slot, std::memory_order_relaxed);

    if (warmPool == nullptr)
        return;

    if (warmPool->isWarm (slot))
    {
        if (auto* instance = warmPool->acquire (slot); instance != nullptr)
        {
            (void) finishArmLocked (slot, instance);
            return;
        }
    }

    // The older previous patch is no longer needed for an instant return.
    // Dropping its pin leaves room for the cold instance inside 1 + K.
    if (pinnedPreviousSlot >= 0 && pinnedPreviousSlot != slot)
    {
        warmPool->release (pinnedPreviousSlot);
        pinnedPreviousSlot = -1;
    }

    // Keep the outgoing patch audible while the new one loads, then let the
    // pool warm the target and its neighbours.
    warmPool->setActiveSlot (slot);
    (void) warmPool->preloadAll();
}

void WarmPoolController::disarm()
{
    const std::lock_guard<std::mutex> lock (armMutex);

    if (switcher != nullptr)
        switcher->disarm();

    requestedSlot.store (-1, std::memory_order_relaxed);
    pendingArmSlot.store (-1, std::memory_order_relaxed);
    activeSlot.store (-1, std::memory_order_relaxed);
}

void WarmPoolController::resetArmState() noexcept
{
    requestedSlot.store (-1, std::memory_order_relaxed);
    activeSlot.store (-1, std::memory_order_relaxed);
    pendingArmSlot.store (-1, std::memory_order_relaxed);
    pinnedActiveSlot = -1;
    pinnedPreviousSlot = -1;
}

void WarmPoolController::replaceRequest (int slot,
                                         const juce::String& requestId,
                                         bool wasActive,
                                         bool wasRequested)
{
    const std::lock_guard<std::mutex> lock (armMutex);

    if (warmPool == nullptr)
        return;

    // Drop our pins before the request retires the old instance. The pool's
    // destroy hook clears the fixed voice's pointer, so the audio thread never
    // reads a deleted source.
    if (pinnedActiveSlot == slot)
        pinnedActiveSlot = -1;

    if (pinnedPreviousSlot == slot)
        pinnedPreviousSlot = -1;

    warmPool->release (slot);

    // A changed id retires the warm instance and queues a fresh one when the
    // slot is protected (the armed slot or a neighbour).
    setRequest (slot, requestId);

    if (wasActive)
    {
        // Re-bind and re-arm through the normal warm hand-off once the new
        // instance is resident.
        pendingArmSlot.store (slot, std::memory_order_relaxed);
        requestedSlot.store (slot, std::memory_order_relaxed);
    }
    else if (wasRequested)
    {
        // Still warming: the pending hand-off will bind the new instance.
        pendingArmSlot.store (slot, std::memory_order_relaxed);
    }
}

//==============================================================================
void WarmPoolController::warmCallback (int slot)
{
    const std::lock_guard<std::mutex> lock (armMutex);

    if (warmPool == nullptr)
        return;

    auto* instance = warmPool->acquire (slot);

    if (instance == nullptr)
        return; // evicted before the hand-off; the next arm retries

    bindVoice (slot, instance);

    if (pendingArmSlot.load (std::memory_order_relaxed) == slot)
    {
        (void) finishArmLocked (slot, instance);
        return;
    }

    warmPool->release (slot);
}

bool WarmPoolController::finishArmLocked (int slot,
                                          AudioEngine::SlotSource* pinnedInstance)
{
    if (warmPool == nullptr || switcher == nullptr || pinnedInstance == nullptr)
        return false;

    // A patch that warmed in the background is restored from its own blob
    // before it becomes audible. A fresh instance already got the state in
    // the pool factory; a resident one whose revision moved on is brought up
    // to date here.
    if (beforeArmHook)
        beforeArmHook (slot);

    bindVoice (slot, pinnedInstance);

    // The only audio-thread effect of arming: one lock-free atomic publish.
    switcher->armSlot (slot);

    const int oldActive = pinnedActiveSlot;
    const int oldPrevious = pinnedPreviousSlot;

    activeSlot.store (slot, std::memory_order_relaxed);
    pendingArmSlot.store (-1, std::memory_order_relaxed);

    if (oldActive == slot)
    {
        // Re-arming the same slot: drop the duplicate pin.
        warmPool->release (slot);
        return true;
    }

    if (oldPrevious >= 0 && oldPrevious != slot)
        warmPool->release (oldPrevious);

    pinnedPreviousSlot = oldActive;
    pinnedActiveSlot = slot;

    if (slot == oldPrevious)
        warmPool->release (slot); // it was already pinned as the previous patch

    return true;
}

} // namespace ihub::session
