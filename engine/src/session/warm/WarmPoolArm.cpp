#include "session/WarmPoolController.h"

#include "audio/SlotSwitcher.h"

namespace ihub::session
{

    void WarmPoolController::arm (int slot)
    {
        if (switcher == nullptr)
            return;

        if (slot < 0)
        {
            disarm ();
            return;
        }

        const std::lock_guard<std::mutex> lock (armMutex);

        pendingArmSlot.store (slot, std::memory_order_relaxed);
        requestedSlot.store (slot, std::memory_order_relaxed);

        if (warmPool == nullptr)
            return;

        if (warmPool->isWarm (slot))
        {
            if (auto* instance = warmPool->acquire (slot); instance != nullptr)
            {
                (void)finishArmLocked (slot, instance);
                return;
            }
        }

        if (!warmPool->hasRequest (slot))
        {
            (void)finishArmLocked (slot, nullptr);
            return;
        }

        if (pinnedPreviousSlot >= 0 && pinnedPreviousSlot != slot)
        {
            warmPool->release (pinnedPreviousSlot);
            pinnedPreviousSlot = -1;
        }

        warmPool->setActiveSlot (slot);
        (void)warmPool->preloadAll ();
    }

    void WarmPoolController::disarm ()
    {
        const std::lock_guard<std::mutex> lock (armMutex);

        if (switcher != nullptr)
            switcher->disarm ();

        requestedSlot.store (-1, std::memory_order_relaxed);
        pendingArmSlot.store (-1, std::memory_order_relaxed);
        activeSlot.store (-1, std::memory_order_relaxed);
    }

    void WarmPoolController::resetArmState () noexcept
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

        if (pinnedActiveSlot == slot)
            pinnedActiveSlot = -1;

        if (pinnedPreviousSlot == slot)
            pinnedPreviousSlot = -1;

        warmPool->release (slot);

        setRequest (slot, requestId);

        if (wasActive)
        {
            pendingArmSlot.store (slot, std::memory_order_relaxed);
            requestedSlot.store (slot, std::memory_order_relaxed);
        }
        else if (wasRequested)
        {
            pendingArmSlot.store (slot, std::memory_order_relaxed);
        }
    }

    void WarmPoolController::warmCallback (int slot)
    {
        SlotStateObserver observer;

        {
            const std::lock_guard<std::mutex> lock (armMutex);
            observer = slotStateObserver;

            if (warmPool != nullptr)
            {
                if (auto* instance = warmPool->acquire (slot); instance != nullptr)
                {
                    bindVoice (slot, instance);

                    if (pendingArmSlot.load (std::memory_order_relaxed) == slot)
                        (void)finishArmLocked (slot, instance);
                    else
                        warmPool->release (slot);
                }
            }
        }

        if (observer != nullptr)
        {
            try
            {
                observer (slot);
            }
            catch (...)
            {
            }
        }
    }

    bool WarmPoolController::finishArmLocked (int slot, AudioEngine::SlotSource* pinnedInstance)
    {
        if (warmPool == nullptr || switcher == nullptr)
            return false;

        if (beforeArmHook)
            beforeArmHook (slot);

        bindVoice (slot, pinnedInstance);

        switcher->armSlot (slot);

        const int oldActive = pinnedActiveSlot;
        const int oldPrevious = pinnedPreviousSlot;

        activeSlot.store (slot, std::memory_order_relaxed);
        pendingArmSlot.store (-1, std::memory_order_relaxed);

        if (oldActive == slot)
        {
            warmPool->release (slot);
            return true;
        }

        if (oldPrevious >= 0 && oldPrevious != slot)
            warmPool->release (oldPrevious);

        pinnedPreviousSlot = oldActive;
        pinnedActiveSlot = slot;

        if (slot == oldPrevious)
            warmPool->release (slot);

        return true;
    }

}
