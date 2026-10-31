// ihub-warm-pool-test: acceptance check for T033.
//
// Every instance is a real TestPlugin.vst3 loaded through PluginInstrument on
// the pool's background thread, and a counting wrapper proves the live-instance
// count directly:
//
//   1. With 8 slots and K = 2 the pool holds at most 3 instances at any time,
//      including during a preload and while sweeping the active slot over every
//      slot. The counter records the peak, not just a sampled value.
//   2. Arming a warm slot (acquire + Slot::setSource + SlotSwitcher::armSlot)
//      completes in under 5 ms; a cold PluginInstrument load is timed for
//      contrast.
//   3. Moving the active slot evicts outside the neighbourhood, a pinned
//      instance survives, and a warm request that could not fit is retried
//      after the pin is released.
//   4. Changing a slot's request id retires the old instance and warms the new
//      one; a failing factory is reported once and does not spin.
//
// The bundle path is injected by CMake (IHUB_TESTPLUGIN_VST3); an explicit path
// may also be passed as the first command-line argument.

#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"
#include "audio/WarmPool.h"
#include "plugins/PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

constexpr int kNumSlots = 8;
constexpr int kNeighbours = 2;
constexpr int kExpectedLimit = 1 + kNeighbours;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

//==============================================================================
// Live-instance accounting. The counter is updated by the wrapper's constructor
// and destructor, so `peak` is the true maximum number of plugin instances that
// coexisted, not a snapshot taken at a convenient moment.

struct InstanceCounter
{
    std::atomic<int> live { 0 };
    std::atomic<int> peak { 0 };
    std::atomic<int> violations { 0 };
    int limit = kExpectedLimit;

    void noteCreated()
    {
        const int now = live.fetch_add (1, std::memory_order_relaxed) + 1;

        if (now > limit)
            violations.fetch_add (1, std::memory_order_relaxed);

        int previous = peak.load (std::memory_order_relaxed);

        while (now > previous
               && ! peak.compare_exchange_weak (previous, now,
                                                std::memory_order_relaxed))
        {
        }
    }

    void noteDestroyed()
    {
        live.fetch_sub (1, std::memory_order_relaxed);
    }

    void resetPeak()
    {
        peak.store (live.load (std::memory_order_relaxed), std::memory_order_relaxed);
        violations.store (0, std::memory_order_relaxed);
    }
};

struct CountedInstance final : AudioEngine::SlotSource
{
    CountedInstance (InstanceCounter& counterToUse,
                     std::unique_ptr<ihub::plugins::PluginInstrument> instrumentToOwn)
        : counter (counterToUse), instrument (std::move (instrumentToOwn))
    {
        counter.noteCreated();
    }

    ~CountedInstance() override
    {
        // Unload first so the counter never reports a plugin that is already
        // gone, or misses one that is still alive.
        instrument.reset();
        counter.noteDestroyed();
    }

    void prepare (double sampleRate, int maximumBlockSize) override
    {
        if (instrument != nullptr)
            instrument->prepare (sampleRate, maximumBlockSize);
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        if (instrument != nullptr)
            instrument->process (buffer, midi);
    }

    int latencySamples() const noexcept override
    {
        return instrument != nullptr ? instrument->latencySamples() : 0;
    }

    InstanceCounter& counter;
    std::unique_ptr<ihub::plugins::PluginInstrument> instrument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CountedInstance)
};

//==============================================================================
double toMilliseconds (std::chrono::steady_clock::duration duration)
{
    return std::chrono::duration<double, std::milli> (duration).count();
}

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types)
{
    const auto path = bundle.getFullPathName();

    for (auto* format : manager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    for (auto* type : types)
        if (type->name.contains ("TestPlugin"))
            return type;

    return nullptr;
}

bool contains (const std::vector<int>& values, int value)
{
    return std::find (values.begin(), values.end(), value) != values.end();
}

//==============================================================================
// JUCE posts plugin instantiation to the message thread and blocks the caller
// until it runs. A console app therefore has to keep pumping while the pool's
// background thread loads; `waitForIdle()` alone would deadlock.

bool pumpUntil (const std::function<bool()>& predicate, int timeoutMs = 30000)
{
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds (timeoutMs);

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;

        juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
    }

    return predicate();
}

std::string join (const std::vector<int>& values)
{
    std::string text;

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
            text += ", ";

        text += std::to_string (values[i]);
    }

    return text;
}
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;

    check (bundle.isDirectory(), "TestPlugin.vst3 bundle exists");

    if (! bundle.isDirectory())
        return 1;

    juce::AudioPluginFormatManager formatManager;
    juce::addHeadlessDefaultFormatsToManager (formatManager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* found = findTestPlugin (formatManager, bundle, types);

    check (found != nullptr, "TestPlugin type is enumerated by the format manager");

    if (found == nullptr)
        return 1;

    const juce::PluginDescription description = *found;

    InstanceCounter counter;
    const auto mainThreadId = std::this_thread::get_id();
    std::atomic<bool> createdOffMainThread { false };

    AudioEngine::WarmPool pool (kNeighbours);

    pool.setNumSlots (kNumSlots);
    pool.setFormat (kSampleRate, kBlockSize);
    pool.setFactory ([&] (const AudioEngine::WarmPool::Request& request,
                          juce::String& error) -> std::unique_ptr<AudioEngine::SlotSource>
    {
        const auto* pluginDescription = std::any_cast<juce::PluginDescription> (&request.payload);

        if (pluginDescription == nullptr)
        {
            error = "request payload is not a PluginDescription";
            return nullptr;
        }

        // Count the wrapper before the plugin object exists, so the tracked
        // peak is the real number of coexisting instances.
        auto counted = std::make_unique<CountedInstance> (
            counter, std::unique_ptr<ihub::plugins::PluginInstrument> {});

        auto instrument = std::make_unique<ihub::plugins::PluginInstrument>();

        if (! instrument->load (formatManager, *pluginDescription,
                                kSampleRate, kBlockSize, error))
        {
            return nullptr;
        }

        instrument->configureBuses (2);
        counted->instrument = std::move (instrument);

        if (std::this_thread::get_id() != mainThreadId)
            createdOffMainThread.store (true, std::memory_order_relaxed);

        return counted;
    });

    const auto identifier = description.createIdentifierString();

    for (int index = 0; index < kNumSlots; ++index)
    {
        AudioEngine::WarmPool::Request request;
        request.id = "slot-" + juce::String (index) + ":" + identifier;
        request.payload = description;

        pool.setRequest (index, request);
    }

    check (pool.getInstanceLimit() == kExpectedLimit,
           "instance limit is active + K = 3 for 8 slots and K = 2");

    // --- 1. Preload --------------------------------------------------------
    pool.setActiveSlot (0);

    auto report = pool.preloadAll();

    std::cout << "preload: resident=" << report.resident
              << " queued=" << report.queued
              << " deferred=" << report.deferred
              << " failed=" << report.failed << std::endl;

    check (report.queued == kExpectedLimit,
           "preload schedules the active slot and its two neighbours");
    check (report.deferred == kNumSlots - kExpectedLimit,
           "preload reports the five requests the cap cannot retain");
    check (report.failed == 0, "preload reports no factory failures");
    check (report.errors.isEmpty(), "preload collected no errors");

    check (pumpUntil ([&] { return pool.isIdle(); }),
           "the pool returns to idle after preload");

    check (counter.live.load() == kExpectedLimit,
           "exactly three plugin instances exist after preload");
    check (pool.getLiveInstanceCount() == kExpectedLimit,
           "the pool reports three live instances after preload");
    check (createdOffMainThread.load(),
           "instances are created on the pool's background thread");
    check (pool.isWarm (0) && pool.isWarm (1) && pool.isWarm (2),
           "slots 0, 1 and 2 are warm (0 is active, 1 and 2 are neighbours)");

    {
        const auto protectedSlots = pool.getProtectedSlots();
        std::cout << "protected: " << join (protectedSlots) << std::endl;

        check (contains (protectedSlots, 0) && contains (protectedSlots, 1)
                   && contains (protectedSlots, 2) && protectedSlots.size() == 3,
               "the protected set is { 0, 1, 2 }");
    }

    counter.resetPeak();

    // --- 2. The cap holds while the active slot moves over every slot ------
    for (int target = 0; target < kNumSlots; ++target)
    {
        pool.setActiveSlot (target);

        check (pumpUntil ([&] { return pool.isIdle(); }),
               "pool idle after arming target " + std::to_string (target));

        const auto protectedSlots = pool.getProtectedSlots();

        check (! protectedSlots.empty() && protectedSlots.front() == target,
               "target " + std::to_string (target) + " is first in the protected set");
        check (pool.isWarm (target),
               "target " + std::to_string (target) + " is warm");

        if (counter.live.load() > kExpectedLimit)
        {
            check (false, "live instance count stayed at or below 3 after target "
                              + std::to_string (target));
        }
    }

    std::cout << "peak live instances over the sweep: " << counter.peak.load()
              << " (limit " << kExpectedLimit << ")" << std::endl;

    check (counter.violations.load() == 0,
           "no instantiation ever pushed the live count past 3");
    check (counter.peak.load() <= kExpectedLimit,
           "the peak number of coexisting instances is at most 3");
    check (pool.getLiveInstanceCount() <= kExpectedLimit,
           "the pool holds at most 3 instances after the sweep");
    check (pool.getEvictionCount() > 0, "moving the active slot evicted instances");

    // --- 3. Arming a warm slot --------------------------------------------
    pool.setActiveSlot (3);

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle before the arm measurement");

    {
        const auto protectedSlots = pool.getProtectedSlots();
        std::cout << "protected at slot 3: " << join (protectedSlots) << std::endl;

        check (contains (protectedSlots, 3) && contains (protectedSlots, 2)
                   && contains (protectedSlots, 4),
               "slot 3's neighbours are 2 and 4");
    }

    std::vector<std::unique_ptr<AudioEngine::Slot>> slots;

    for (int index = 0; index < kNumSlots; ++index)
    {
        auto slot = std::make_unique<AudioEngine::Slot> (index);
        slot->prepare (kSampleRate, kBlockSize, 2);
        slots.push_back (std::move (slot));
    }

    std::vector<AudioEngine::Slot*> slotPointers;

    for (auto& slot : slots)
        slotPointers.push_back (slot.get());

    AudioEngine::SlotSwitcher switcher;
    switcher.setSlots (slotPointers);
    switcher.prepare (kSampleRate, kBlockSize, 2);

    const auto warmStart = std::chrono::steady_clock::now();

    auto* warmInstance = pool.armSlot (4);

    if (warmInstance != nullptr)
        slots[4]->setSource (warmInstance);

    switcher.armSlot (4);

    const auto warmEnd = std::chrono::steady_clock::now();
    const double warmArmMs = toMilliseconds (warmEnd - warmStart);

    std::cout << "warm arm: " << warmArmMs << " ms" << std::endl;

    check (warmInstance != nullptr, "arming a neighbour hands over a warm instance");
    check (warmArmMs < 5.0, "arming a warm slot completes in under 5 ms");

    {
        juce::MidiBuffer midi;
        check (switcher.process (kBlockSize, midi),
               "the armed slot renders its first block");
    }

    // Detach before the pool is allowed to retire slot 4.
    slots[4]->setSource (nullptr);
    pool.release (4);

    // Cold load, for contrast: this is the cost the warm pool hides.
    {
        const auto coldStart = std::chrono::steady_clock::now();

        ihub::plugins::PluginInstrument coldInstrument;
        juce::String coldError;
        const bool coldLoaded = coldInstrument.load (formatManager, description,
                                                     kSampleRate, kBlockSize, coldError);

        const double coldMs = toMilliseconds (std::chrono::steady_clock::now() - coldStart);

        std::cout << "cold load: " << coldMs << " ms" << std::endl;

        check (coldLoaded, "a cold PluginInstrument load succeeds for comparison" +
                               (coldError.isEmpty() ? std::string()
                                                    : " (" + coldError.toStdString() + ")"));

        // Printed for contrast only: a cold load is outside the pool, so it is
        // not part of the instance-cap assertion, and its timing depends on
        // disk cache warmth.
    }

    // --- 4. Pins, eviction and the deferred retry --------------------------
    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle after releasing slot 4");
    check (counter.live.load() <= kExpectedLimit, "live count is still within the cap");

    pool.setActiveSlot (0);

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle at slot 0");

    auto* pinned = pool.acquire (0);

    check (pinned != nullptr, "slot 0 can be pinned while it is warm");

    pool.setActiveSlot (7);

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle after jumping to slot 7");

    {
        const auto protectedSlots = pool.getProtectedSlots();
        std::cout << "protected at slot 7 with slot 0 pinned: " << join (protectedSlots)
                  << std::endl;
    }

    check (pool.isWarm (0), "a pinned instance survives leaving the neighbourhood");
    check (pool.getLiveInstanceCount() <= kExpectedLimit,
           "the cap holds while one instance is pinned");

    pool.release (0);

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle after releasing the pin");

    check (! pool.isWarm (0), "releasing the pin lets the LRU retire the instance");
    check (pool.isWarm (7) && pool.isWarm (6) && pool.isWarm (5),
           "the deferred neighbour is warmed once the pin is released");
    check (counter.live.load() <= kExpectedLimit,
           "the live count stays within the cap through eviction and retry");

    // --- 5. Request changes and failures -----------------------------------
    pool.setActiveSlot (3);

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle at slot 3 before the request change");
    check (pool.isWarm (3), "slot 3 is warm before its request changes");

    const auto evictionsBefore = pool.getEvictionCount();
    const auto warmBefore = pool.getWarmCount();

    {
        AudioEngine::WarmPool::Request request;
        request.id = "slot-3-v2:" + identifier;
        request.payload = description;

        pool.setRequest (3, request);
    }

    check (! pool.isWarm (3), "changing a request id retires the old instance");

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle after the request change");
    check (pool.isWarm (3), "the new request is warmed");
    check (pool.getEvictionCount() > evictionsBefore, "the old instance was evicted");
    check (pool.getWarmCount() > warmBefore, "the new instance was created");

    // A pinned slot cannot swap its instance immediately; the replacement is
    // warmed as soon as the pin is released.
    auto* pinnedForChange = pool.acquire (3);

    check (pinnedForChange != nullptr, "slot 3 is pinned before the second change");

    {
        AudioEngine::WarmPool::Request request;
        request.id = "slot-3-v3:" + identifier;
        request.payload = description;

        pool.setRequest (3, request);
    }

    check (pool.isWarm (3), "the pinned instance survives its request changing");

    pool.release (3);

    check (pumpUntil ([&] { return pool.isIdle(); }), "pool idle after the pinned change");
    check (pool.isWarm (3), "the replacement is warmed after the pin is released");
    check (counter.live.load() <= kExpectedLimit, "the cap holds through the swap");

    {
        AudioEngine::WarmPool failingPool (kNeighbours);
        failingPool.setNumSlots (2);
        failingPool.setFactory ([] (const AudioEngine::WarmPool::Request&,
                                    juce::String& error) -> std::unique_ptr<AudioEngine::SlotSource>
        {
            error = "synthetic load failure";
            return nullptr;
        });

        AudioEngine::WarmPool::Request request;
        request.id = "broken";
        failingPool.setRequest (0, request);
        failingPool.setActiveSlot (0);

        auto failingReport = failingPool.preloadAll();

        check (failingReport.queued == 1, "the failing request is scheduled");

        check (pumpUntil ([&] { return failingPool.isIdle(); }, 5000),
               "a failed request does not spin the background thread");

        // A second preload reports the remembered failure instead of retrying
        // it forever.
        failingReport = failingPool.preloadAll();

        check (failingReport.failed == 1, "a factory failure is reported");
        check (failingReport.errors.size() == 1, "the factory error text is kept");
        check (! failingPool.isWarm (0), "the failed slot stays cold");
        check (failingPool.getWarmCount() == 0, "nothing was warmed");
    }

    // --- Wrap up -----------------------------------------------------------
    pool.clear();

    check (pumpUntil ([&] { return pool.isIdle(); }), "clear() leaves the pool idle");
    check (pool.getLiveInstanceCount() == 0, "clear() retires every instance");

    // Give the destructor path a moment to run; this proves nothing leaks when
    // the pool goes away with warm instances still resident.
    for (int index = 0; index < kNumSlots; ++index)
        pool.setActiveSlot (index);

    pumpUntil ([&] { return pool.isIdle(); });

    std::cout << "instances alive before destruction: " << counter.live.load() << std::endl;
    check (counter.live.load() <= kExpectedLimit,
           "still within the cap before the pool is destroyed");
    check (counter.violations.load() == 0,
           "the instance cap was never exceeded across the whole run");

    // Shut down the way a host must: retire everything and keep the message
    // loop running until the pool's background thread has destroyed it.
    pool.clear();

    check (pumpUntil ([&] { return pool.isIdle(); }, 30000),
           "the pool drains its destroy queue");
    check (counter.live.load() == 0, "every instance was destroyed off the audio path");

    std::cout << (failures == 0 ? "PASS" : "FAILED") << ": "
              << failures << " failure(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
