#include "Session.h"

#include <utility>

namespace ihub
{

int Session::getWarmInstanceLimit() const noexcept
{
    return warmPool.getPool() != nullptr
               ? warmPool.getInstanceLimit()
               : juce::jmin (getLogicalSlotCount(), 1 + config.neighbourCount);
}

int Session::getLiveInstrumentCount() const noexcept
{
    return warmPool.getLiveInstrumentCount();
}

int Session::getPeakInstrumentCount() const noexcept
{
    return warmPool.getPeakInstrumentCount();
}

bool Session::isSlotWarm (int slot) const noexcept
{
    return warmPool.isWarm (slot);
}

SessionWarmState Session::getWarmState (int slot) const noexcept
{
    if (slot < 0 || slot >= getNumSlots())
        return SessionWarmState::cold;

    if (warmPool.getPendingSlot() == slot)
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
