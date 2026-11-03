#include "ProtectedWarmFixtures.h"

#include "Session.h"

#include <algorithm>
#include <string>

namespace ProtectedWarmTest
{

void runSessionScenario()
{
    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig(), error))
    {
        check (false, "the eight-track session builds: " + error.toStdString());
        return;
    }

    check (session.getNumSlots() == kSlots, "the session has eight slots");
    check (session.getWarmProtectedCount() == kSlots,
           "the session reports eight protected slots");
    check (session.getWarmProtectedSlots() == slotRange (kSlots),
           "the session reports slots 0-7 as protected");
    check (session.getWarmInstanceLimit() == kSlots,
           "the session live limit is the protected count");

    bool allWarm = true;

    for (int slot = 0; slot < kSlots; ++slot)
        allWarm = session.waitForSlotWarm (slot, 10000) && allWarm;

    check (allWarm, "all eight protected slots warm");
    check (session.waitForWarmIdle (10000), "the session preload returns to idle");

    const auto report = session.preloadAll();

    check (report.resident == kSlots && report.queued == 0
               && report.deferred == 0 && report.failed == 0,
           "preloadAll reports all eight protected slots warm");
    check (session.getWarmCreationCount() == kSlots,
           "the session created exactly eight warm instances");
    check (session.getWarmEvictionCount() == 0,
           "preloading the protected set evicts nothing");
    check (session.getLiveInstrumentCount() == kSlots,
           "the session reports eight live instances");

    const auto creationBaseline = session.getWarmCreationCount();
    const auto evictionBaseline = session.getWarmEvictionCount();
    bool everyArmActive = true;
    bool neverWarming = true;
    int worstLive = 0;

    for (int index = 0; index < kSwitches; ++index)
    {
        const int target = index % kSlots;

        if (session.getWarmState (target) == ihub::SessionWarmState::warming)
            neverWarming = false;

        session.armSlot (target);

        everyArmActive = everyArmActive
                      && session.getWarmState (target)
                             == ihub::SessionWarmState::active
                      && session.getWarmStateName (target)
                             != std::string ("warming");

        if (session.getWarmState (target) == ihub::SessionWarmState::warming)
            neverWarming = false;

        worstLive = juce::jmax (worstLive, session.getLiveInstrumentCount());
    }

    check (session.getWarmState ((kSwitches - 1) % kSlots)
               == ihub::SessionWarmState::active,
           "the session ends with a protected slot active");
    check (everyArmActive, "arming every protected slot reports active");
    check (neverWarming, "no protected arm ever reports warming");
    check (worstLive <= kSlots,
           "the live count never exceeds the protected count");
    check (session.getWarmCreationCount() == creationBaseline,
           "100 session switches create no new instances");
    check (session.getWarmEvictionCount() == evictionBaseline,
           "100 session switches evict nothing");
    check (session.getLiveInstrumentCount() == kSlots,
           "the session still holds eight live instances");
    check (session.getPeakInstrumentCount() <= kSlots,
           "the peak live count stays within the protected count");

    const auto protectedSlots = slotRange (kSlots);

    check (std::all_of (protectedSlots.begin(), protectedSlots.end(),
                        [&session] (int slot) { return session.isSlotWarm (slot); }),
           "every protected slot stays warm after 100 switches");
}

}
