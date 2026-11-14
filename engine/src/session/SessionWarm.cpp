#include "Session.h"

#include <utility>

namespace ihub
{

    int Session::getWarmInstanceLimit () const noexcept
    {
        if (warmPool.getPool () != nullptr)
            return warmPool.getInstanceLimit ();

        const int desired = config.warmSlots.empty ()
                                ? 1 + config.neighbourCount
                                : juce::jmax (1, static_cast<int> (config.warmSlots.size ()));

        return juce::jmin (
            getLogicalSlotCount (), AudioEngine::WarmPool::maximumWarmInstances, desired);
    }

    int Session::getLiveInstrumentCount () const noexcept
    {
        return warmPool.getLiveInstrumentCount ();
    }

    int Session::getWarmProtectedCount () const noexcept
    {
        return warmPool.getProtectedCount ();
    }

    std::vector<int> Session::getWarmProtectedSlots () const
    {
        return warmPool.getProtectedSlots ();
    }

    std::uint64_t Session::getWarmCreationCount () const noexcept
    {
        return warmPool.getWarmCount ();
    }

    std::uint64_t Session::getWarmEvictionCount () const noexcept
    {
        return warmPool.getEvictionCount ();
    }

    AudioEngine::WarmPool::PreloadReport Session::preloadAll ()
    {
        return warmPool.preloadAll ();
    }

    bool Session::waitForWarmIdle (int timeoutMs)
    {
        return warmPool.waitForIdle (timeoutMs);
    }

    int Session::getPeakInstrumentCount () const noexcept
    {
        return warmPool.getPeakInstrumentCount ();
    }

    bool Session::isSlotWarm (int slot) const noexcept
    {
        return warmPool.isWarm (slot);
    }

    SessionWarmState Session::getWarmState (int slot) const noexcept
    {
        if (slot < 0 || slot >= getNumSlots ())
            return SessionWarmState::cold;

        if (warmPool.getPendingSlot () == slot)
            return SessionWarmState::warming;

        if (warmPool.isActive (slot))
            return SessionWarmState::active;

        if (warmPool.isWarming (slot))
            return SessionWarmState::warming;

        if (warmPool.isWarm (slot))
            return SessionWarmState::warm;

        return SessionWarmState::cold;
    }

    const char* Session::getWarmStateName (int slot) const noexcept
    {
        return toString (getWarmState (slot));
    }

    void Session::setSlotStateObserver (SlotStateObserver observer)
    {
        {
            const std::lock_guard<std::mutex> lock (slotObserverMutex);
            slotStateObserver = observer;
        }

        warmPool.setSlotStateObserver (std::move (observer));
    }

    bool Session::waitForSlotWarm (int slot, int timeoutMs)
    {
        return warmPool.waitForSlotWarm (slot, timeoutMs);
    }

    bool Session::waitForSlotActive (int slot, int timeoutMs)
    {
        return warmPool.waitForSlotActive (slot, timeoutMs);
    }

}
