#include "WarmPoolController.h"

#include "session/warm/WarmPoolVoice.h"

#include <algorithm>
#include <any>
#include <iostream>
#include <utility>

namespace ihub::session
{

namespace
{
constexpr int shutdownBudgetMs = 1000;
}

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
        voices.push_back (std::make_unique<detail::WarmPoolVoice>());
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
                                    AudioEngine::SlotSwitcher* switcherToUse,
                                    CancelLoads cancel)
{
    sourceFactory = std::move (factory);
    destroyObserver = std::move (destroy);
    beforeArmHook = std::move (beforeArm);
    cancelLoads = std::move (cancel);
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
        if (destroyObserver)
            destroyObserver (instance);

        clearVoice (&instance);
    });

    warmPool->setWarmCallback ([this] (int slot)
    {
        warmCallback (slot);
    });
}

void WarmPoolController::setSlotStateObserver (SlotStateObserver observer)
{
    const std::lock_guard<std::mutex> lock (armMutex);
    slotStateObserver = std::move (observer);
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

void WarmPoolController::setProtectedSlots (const std::vector<int>& slots)
{
    if (warmPool != nullptr)
        warmPool->setProtectedSlots (slots);
}

std::vector<int> WarmPoolController::getProtectedSlots() const
{
    return warmPool != nullptr ? warmPool->getProtectedSlots() : std::vector<int>();
}

int WarmPoolController::getProtectedCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getProtectedCount() : 0;
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

AudioEngine::WarmPool::PreloadReport WarmPoolController::preloadAll()
{
    if (warmPool != nullptr)
        return warmPool->preloadAll();

    return {};
}

void WarmPoolController::shutdown() noexcept
{
    if (warmPool == nullptr)
        return;

    if (cancelLoads)
        cancelLoads();

    warmPool->setWarmCallback (nullptr);

    if (! warmPool->shutdown (shutdownBudgetMs))
    {
        std::cerr << "session: the warm pool did not stop; abandoning its worker"
                  << std::endl;
        (void) warmPool.release();
    }
    else
    {
        warmPool.reset();
    }

    pinnedActiveSlot = -1;
    pinnedPreviousSlot = -1;
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

std::uint64_t WarmPoolController::getWarmCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getWarmCount() : 0;
}

std::uint64_t WarmPoolController::getEvictionCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getEvictionCount() : 0;
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

}
