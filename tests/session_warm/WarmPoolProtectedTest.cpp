#include "ProtectedWarmFixtures.h"

#include "audio/WarmPool.h"

#include <algorithm>
#include <memory>

namespace ProtectedWarmTest
{

void runWarmPoolScenario()
{
    SourceCounters counters;
    AudioEngine::WarmPool pool (2);

    pool.setNumSlots (kUnprotectedSlots);
    pool.setFactory ([&counters] (const AudioEngine::WarmPool::Request&,
                                  juce::String&) -> std::unique_ptr<AudioEngine::SlotSource>
    {
        return std::make_unique<CountingSource> (counters);
    });

    for (int index = 0; index < kUnprotectedSlots; ++index)
    {
        AudioEngine::WarmPool::Request request;
        request.id = "slot-" + juce::String (index);
        pool.setRequest (index, request);
    }

    const auto protectedSlots = slotRange (kSlots);
    pool.setProtectedSlots (protectedSlots);
    pool.setActiveSlot (0);

    check (pool.getProtectedCount() == kSlots,
           "the pool reports eight protected slots");
    check (pool.getProtectedSlots() == protectedSlots,
           "the protected list keeps the configured order");
    check (pool.getInstanceLimit() == kSlots,
           "the live limit covers the protected count");

    auto report = pool.preloadAll();

    check (report.resident + report.queued == kSlots,
           "preloadAll schedules all eight protected slots");
    check (pumpUntil ([&pool] { return pool.isIdle(); }),
           "the protected preload returns to idle");

    report = pool.preloadAll();

    check (report.resident == kSlots && report.queued == 0
               && report.deferred == kUnprotectedSlots - kSlots
               && report.failed == 0,
           "preloadAll reports all eight protected slots warm");
    check (counters.created.load() == kSlots && counters.live.load() == kSlots,
           "exactly eight instances exist after the protected preload");
    check (pool.getLiveInstanceCount() == kSlots,
           "the pool reports eight live instances");
    check (std::all_of (protectedSlots.begin(), protectedSlots.end(),
                        [&pool] (int slot) { return pool.isWarm (slot); }),
           "every protected slot is warm");

    const auto warmBaseline = pool.getWarmCount();
    const auto evictionBaseline = pool.getEvictionCount();
    bool everyTargetWarm = true;

    for (int index = 0; index < kSwitches; ++index)
    {
        const int target = index % kSlots;
        pool.setActiveSlot (target);
        everyTargetWarm = everyTargetWarm && pool.isWarm (target);
    }

    check (pumpUntil ([&pool] { return pool.isIdle(); }),
           "the protected switch run returns to idle");
    check (everyTargetWarm, "every protected switch target stays warm");
    check (pool.getWarmCount() == warmBaseline,
           "100 protected switches create no new instances");
    check (pool.getEvictionCount() == evictionBaseline,
           "100 protected switches evict nothing");
    check (counters.created.load() == kSlots && counters.live.load() == kSlots,
           "the instance count is unchanged after 100 protected switches");

    pool.setActiveSlot (12);

    check (pool.getProtectedCount() == kSlots,
           "an unprotected active slot does not change the protected list");
    check (pool.getInstanceLimit() == kSlots + 1,
           "the live limit grows to hold an unprotected active slot");

    (void) pool.preloadAll();

    check (pumpUntil ([&pool] { return pool.isIdle(); }),
           "the unprotected active slot warms");
    check (pool.isWarm (12), "the unprotected active slot is warm");
    check (pool.getLiveInstanceCount() == kSlots + 1,
           "the unprotected active slot counts as a live instance");

    pool.setActiveSlot (0);

    check (pumpUntil ([&pool] { return pool.isIdle(); }),
           "leaving the unprotected slot returns to idle");
    check (! pool.isWarm (12), "the unprotected slot is evicted after leaving");
    check (pool.getLiveInstanceCount() == kSlots,
           "the live count returns to the protected count");
    check (pool.getEvictionCount() > evictionBaseline,
           "only the unprotected slot was evicted");

    pool.setProtectedSlots ({});

    check (pool.getProtectedSlots() == std::vector<int> { 0, 1, 2 },
           "an empty protected list restores the neighbour policy");
    check (pool.getInstanceLimit() == 3,
           "the empty list restores the neighbour instance limit");

    pool.clear();

    check (pumpUntil ([&pool] { return pool.isIdle(); }, 30000),
           "the pool drains after clear");
    check (counters.live.load() == 0, "every instance is destroyed after clear");

    AudioEngine::WarmPool cappedPool (2);
    cappedPool.setNumSlots (40);
    cappedPool.setProtectedSlots (slotRange (40));

    check (cappedPool.getProtectedCount() == 40,
           "a wide protected list keeps all forty slots");
    check (cappedPool.getInstanceLimit()
               == AudioEngine::WarmPool::maximumWarmInstances,
           "the protected live limit is capped at thirty-two");
}

}
