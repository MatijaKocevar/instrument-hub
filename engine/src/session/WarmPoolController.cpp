#include "WarmPoolController.h"

#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <any>
#include <utility>

namespace ihub::session
{

namespace
{
/**
    The permanent source of one fixed engine slot.

    The engine slot never changes its `SlotSource*`, so the graph really is
    fixed. The actual patch instrument lives in the warm pool and is published
    here with an atomic pointer store: arming is one pointer hand-over, not a
    graph edit. While no instance is bound the proxy renders silence.
*/
class Voice final : public AudioEngine::SlotSource
{
public:
    void prepare (double, int) override {}

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        auto* instance = delegate.load (std::memory_order_acquire);

        if (instance == nullptr)
        {
            buffer.clear();
            return;
        }

        instance->process (buffer, midi);
    }

    int latencySamples() const noexcept override
    {
        return latency.load (std::memory_order_relaxed);
    }

    bool acceptsDeviceInput() const noexcept override
    {
        return acceptsInput.load (std::memory_order_relaxed);
    }

    void setInputBlock (const float* const* channelData, int numInputs) noexcept override
    {
        auto* instance = delegate.load (std::memory_order_acquire);

        if (instance != nullptr && acceptsInput.load (std::memory_order_relaxed))
            instance->setInputBlock (channelData, numInputs);
    }

    bool bypassesFxChain() const noexcept override
    {
        return bypassesChain.load (std::memory_order_relaxed);
    }

    void bind (AudioEngine::SlotSource* instance) noexcept
    {
        int latencySamples = 0;
        bool accepts = false;
        bool bypasses = false;

        if (instance != nullptr)
        {
            latencySamples = instance->latencySamples();
            accepts = instance->acceptsDeviceInput();
            bypasses = instance->bypassesFxChain();
        }

        latency.store (latencySamples, std::memory_order_relaxed);
        acceptsInput.store (accepts, std::memory_order_relaxed);
        bypassesChain.store (bypasses, std::memory_order_relaxed);
        delegate.store (instance, std::memory_order_release);
    }

    void clearIf (AudioEngine::SlotSource* instance) noexcept
    {
        if (delegate.load (std::memory_order_acquire) != instance)
            return;

        delegate.store (nullptr, std::memory_order_release);
        latency.store (0, std::memory_order_relaxed);
        acceptsInput.store (false, std::memory_order_relaxed);
        bypassesChain.store (false, std::memory_order_relaxed);
    }

    std::atomic<AudioEngine::SlotSource*> delegate { nullptr };
    std::atomic<int> latency { 0 };
    std::atomic<bool> acceptsInput { false };
    std::atomic<bool> bypassesChain { false };
};
} // namespace

WarmPoolController::WarmPoolController() = default;

WarmPoolController::~WarmPoolController()
{
    shutdown();
}

void WarmPoolController::createVoices (int slotCount)
{
    voices.clear();
    voices.reserve (static_cast<std::size_t> (std::max (0, slotCount)));

    for (int slot = 0; slot < slotCount; ++slot)
        voices.push_back (std::make_unique<Voice>());
}

void WarmPoolController::clearVoices() noexcept
{
    voices.clear();
}

AudioEngine::SlotSource* WarmPoolController::getVoiceSource (int slot) noexcept
{
    if (slot < 0 || slot >= static_cast<int> (voices.size()))
        return nullptr;

    return voices[static_cast<std::size_t> (slot)].get();
}

int WarmPoolController::getNumVoices() const noexcept
{
    return static_cast<int> (voices.size());
}

void WarmPoolController::createPool (int neighbourCount)
{
    warmPool = std::make_unique<AudioEngine::WarmPool> (neighbourCount);
}

void WarmPoolController::configure (SourceFactory factory,
                                    DestroyObserver destroy,
                                    BeforeArmHook beforeArm,
                                    AudioEngine::SlotSwitcher* switcherToUse)
{
    sourceFactory = std::move (factory);
    destroyObserver = std::move (destroy);
    beforeArmHook = std::move (beforeArm);
    switcher = switcherToUse;

    if (warmPool == nullptr)
        return;

    warmPool->setFactory ([this] (const AudioEngine::WarmPool::Request& request,
                                  juce::String& error)
    {
        int slotIndex = -1;

        try
        {
            slotIndex = std::any_cast<int> (request.payload);
        }
        catch (...)
        {
            error = "warm request has no slot payload";
            return std::unique_ptr<AudioEngine::SlotSource>();
        }

        if (! sourceFactory)
        {
            error = "the warm pool has no source factory";
            return std::unique_ptr<AudioEngine::SlotSource>();
        }

        return sourceFactory (slotIndex, error);
    });

    warmPool->setDestroyHook ([this] (AudioEngine::SlotSource& instance)
    {
        // The session captures plugin state and unregisters the instance
        // before it dies; then the fixed voice forgets it.
        if (destroyObserver)
            destroyObserver (instance);

        clearVoice (&instance);
    });

    warmPool->setWarmCallback ([this] (int slot)
    {
        warmCallback (slot);
    });
}

void WarmPoolController::setFormat (double sampleRate, int maximumBlockSize)
{
    if (warmPool != nullptr)
        warmPool->setFormat (sampleRate, maximumBlockSize);
}

void WarmPoolController::setNumSlots (int slotCount)
{
    if (warmPool != nullptr)
        warmPool->setNumSlots (slotCount);
}

void WarmPoolController::setRequest (int slot, const juce::String& requestId)
{
    if (warmPool == nullptr)
        return;

    AudioEngine::WarmPool::Request request;
    request.id = requestId;
    request.payload = slot;
    warmPool->setRequest (slot, std::move (request));
}

void WarmPoolController::setActiveSlot (int slot)
{
    if (warmPool != nullptr)
        warmPool->setActiveSlot (slot);
}

void WarmPoolController::preloadAll()
{
    if (warmPool != nullptr)
        (void) warmPool->preloadAll();
}

void WarmPoolController::shutdown() noexcept
{
    if (warmPool == nullptr)
        return;

    warmPool->setWarmCallback (nullptr);
    warmPool->clear();
    (void) waitForWarmIdle (2000);
    warmPool.reset();

    pinnedActiveSlot = -1;
    pinnedPreviousSlot = -1;
}

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

int WarmPoolController::getRequestedSlot() const noexcept
{
    return requestedSlot.load (std::memory_order_relaxed);
}

int WarmPoolController::getActiveSlot() const noexcept
{
    return activeSlot.load (std::memory_order_relaxed);
}

int WarmPoolController::getPendingSlot() const noexcept
{
    return pendingArmSlot.load (std::memory_order_relaxed);
}

bool WarmPoolController::isWarm (int slot) const noexcept
{
    return warmPool != nullptr && warmPool->isWarm (slot);
}

bool WarmPoolController::isWarming (int slot) const noexcept
{
    return warmPool != nullptr && warmPool->isWarming (slot);
}

bool WarmPoolController::isActive (int slot) const noexcept
{
    return activeSlot.load (std::memory_order_relaxed) == slot;
}

int WarmPoolController::getInstanceLimit() const noexcept
{
    return warmPool != nullptr ? warmPool->getInstanceLimit() : 0;
}

int WarmPoolController::getLiveInstrumentCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getLiveInstanceCount() : 0;
}

int WarmPoolController::getPeakInstrumentCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getPeakLiveInstanceCount() : 0;
}

int WarmPoolController::getNeighbourCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getNeighbourCount() : 0;
}

void WarmPoolController::setNeighbourCount (int count)
{
    if (warmPool != nullptr)
        warmPool->setNeighbourCount (count);
}

bool WarmPoolController::waitForSlotWarm (int slot, int timeoutMs)
{
    if (slot < 0 || slot >= getNumVoices())
        return false;

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (std::max (0, timeoutMs));

    for (;;)
    {
        if (warmPool != nullptr && warmPool->isWarm (slot))
        {
            const std::lock_guard<std::mutex> lock (armMutex);

            if (auto* instance = warmPool->acquire (slot); instance != nullptr)
            {
                bindVoice (slot, instance);
                warmPool->release (slot);
                return true;
            }
        }

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

        pumpMessages (2);
    }
}

bool WarmPoolController::waitForSlotActive (int slot, int timeoutMs)
{
    if (slot < 0 || slot >= getNumVoices())
        return false;

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (std::max (0, timeoutMs));

    for (;;)
    {
        if (isActive (slot))
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

        pumpMessages (2);
    }
}

bool WarmPoolController::waitForIdle (int timeoutMs)
{
    return waitForWarmIdle (timeoutMs);
}

bool WarmPoolController::pinPairForMeasurement (int fromSlot,
                                                int toSlot,
                                                int savedNeighbourCount)
{
    if (warmPool == nullptr)
        return false;

    const std::lock_guard<std::mutex> lock (armMutex);

    releaseAllPinsLocked();

    // The measurement needs both endpoints resident at once. Temporary
    // neighbours would compete for the 1+K budget with the pinned pair, so
    // shrink the neighbourhood to one (limit 2) for the warm-up and restore it
    // afterwards.
    warmPool->setNeighbourCount (std::min (1, savedNeighbourCount));

    if (! warmSlotBlockingLocked (fromSlot, 5000))
    {
        warmPool->setNeighbourCount (savedNeighbourCount);
        return false;
    }

    auto* fromInstance = warmPool->acquire (fromSlot);

    if (fromInstance == nullptr)
    {
        warmPool->setNeighbourCount (savedNeighbourCount);
        return false;
    }

    if (! warmSlotBlockingLocked (toSlot, 5000))
    {
        warmPool->release (fromSlot);
        warmPool->setNeighbourCount (savedNeighbourCount);
        return false;
    }

    auto* toInstance = warmPool->acquire (toSlot);

    if (toInstance == nullptr)
    {
        warmPool->release (fromSlot);
        warmPool->setNeighbourCount (savedNeighbourCount);
        return false;
    }

    bindVoice (fromSlot, fromInstance);
    bindVoice (toSlot, toInstance);

    pinnedActiveSlot = toSlot;
    pinnedPreviousSlot = fromSlot;
    activeSlot.store (toSlot, std::memory_order_relaxed);
    requestedSlot.store (toSlot, std::memory_order_relaxed);
    pendingArmSlot.store (-1, std::memory_order_relaxed);

    warmPool->setNeighbourCount (savedNeighbourCount);
    return true;
}

void WarmPoolController::bindVoice (int slot, AudioEngine::SlotSource* instance) noexcept
{
    if (slot < 0 || slot >= static_cast<int> (voices.size()))
        return;

    auto* voice = dynamic_cast<Voice*> (voices[static_cast<std::size_t> (slot)].get());

    if (voice != nullptr)
        voice->bind (instance);
}

void WarmPoolController::clearVoice (AudioEngine::SlotSource* instance) noexcept
{
    if (instance == nullptr)
        return;

    for (auto& voiceSource : voices)
        if (auto* voice = dynamic_cast<Voice*> (voiceSource.get()))
            voice->clearIf (instance);
}

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

bool WarmPoolController::warmSlotBlockingLocked (int slot, int timeoutMs)
{
    if (warmPool == nullptr)
        return false;

    if (warmPool->isWarm (slot))
        return true;

    warmPool->setActiveSlot (slot);
    (void) warmPool->preloadAll();

    // A protected neighbour that cannot fit keeps the pool technically busy
    // (deferred), but the target we were asked for may already be resident.
    (void) waitForWarmIdle (timeoutMs);

    if (warmPool->isWarm (slot))
        return true;

    auto* instance = warmPool->acquire (slot);

    if (instance == nullptr)
        return false;

    bindVoice (slot, instance);
    warmPool->release (slot);
    return true;
}

bool WarmPoolController::waitForWarmIdle (int timeoutMs)
{
    if (warmPool == nullptr)
        return true;

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (std::max (0, timeoutMs));

    for (;;)
    {
        if (warmPool->isIdle())
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

        pumpMessages (2);
    }
}

void WarmPoolController::releaseAllPinsLocked()
{
    if (warmPool == nullptr)
    {
        pinnedActiveSlot = -1;
        pinnedPreviousSlot = -1;
        return;
    }

    const int previous = pinnedPreviousSlot;
    const int active = pinnedActiveSlot;

    pinnedPreviousSlot = -1;
    pinnedActiveSlot = -1;

    if (previous >= 0)
        warmPool->release (previous);

    if (active >= 0)
        warmPool->release (active);
}

void WarmPoolController::pumpMessages (int milliseconds)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
        manager->runDispatchLoopUntil (milliseconds);
    else
        juce::Thread::sleep (milliseconds);
   #else
    juce::Thread::sleep (milliseconds);
   #endif
}

} // namespace ihub::session
