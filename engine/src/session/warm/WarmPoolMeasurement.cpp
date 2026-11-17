#include "session/WarmPoolController.h"

#include <juce_events/juce_events.h>

#include <algorithm>

namespace ihub::session {

bool WarmPoolController::waitForSlotWarm(int slot, int timeoutMs) {
  if (slot < 0 || slot >= getNumVoices()) return false;

  const double deadline = juce::Time::getMillisecondCounterHiRes() +
                          static_cast<double>(std::max(0, timeoutMs));

  for (;;) {
    if (warmPool != nullptr && warmPool->isWarm(slot)) {
      const std::lock_guard<std::mutex> lock(armMutex);

      if (auto* instance = warmPool->acquire(slot); instance != nullptr) {
        bindVoice(slot, instance);
        warmPool->release(slot);
        return true;
      }
    }

    if (juce::Time::getMillisecondCounterHiRes() >= deadline) return false;

    pumpMessages(2);
  }
}

bool WarmPoolController::waitForSlotActive(int slot, int timeoutMs) {
  if (slot < 0 || slot >= getNumVoices()) return false;

  const double deadline = juce::Time::getMillisecondCounterHiRes() +
                          static_cast<double>(std::max(0, timeoutMs));

  for (;;) {
    if (isActive(slot)) return true;

    if (juce::Time::getMillisecondCounterHiRes() >= deadline) return false;

    pumpMessages(2);
  }
}

bool WarmPoolController::waitForIdle(int timeoutMs) {
  return waitForWarmIdle(timeoutMs);
}

bool WarmPoolController::pinPairForMeasurement(int fromSlot, int toSlot,
                                               int savedNeighbourCount) {
  if (warmPool == nullptr) return false;

  const std::lock_guard<std::mutex> lock(armMutex);

  releaseAllPinsLocked();

  warmPool->setNeighbourCount(std::min(1, savedNeighbourCount));

  if (!warmSlotBlockingLocked(fromSlot, 5000)) {
    warmPool->setNeighbourCount(savedNeighbourCount);
    return false;
  }

  auto* fromInstance = warmPool->acquire(fromSlot);

  if (fromInstance == nullptr) {
    warmPool->setNeighbourCount(savedNeighbourCount);
    return false;
  }

  if (!warmSlotBlockingLocked(toSlot, 5000)) {
    warmPool->release(fromSlot);
    warmPool->setNeighbourCount(savedNeighbourCount);
    return false;
  }

  auto* toInstance = warmPool->acquire(toSlot);

  if (toInstance == nullptr) {
    warmPool->release(fromSlot);
    warmPool->setNeighbourCount(savedNeighbourCount);
    return false;
  }

  bindVoice(fromSlot, fromInstance);
  bindVoice(toSlot, toInstance);

  pinnedActiveSlot = toSlot;
  pinnedPreviousSlot = fromSlot;
  activeSlot.store(toSlot, std::memory_order_relaxed);
  requestedSlot.store(toSlot, std::memory_order_relaxed);
  pendingArmSlot.store(-1, std::memory_order_relaxed);

  warmPool->setNeighbourCount(savedNeighbourCount);
  return true;
}

bool WarmPoolController::warmSlotBlockingLocked(int slot, int timeoutMs) {
  if (warmPool == nullptr) return false;

  if (warmPool->isWarm(slot)) return true;

  warmPool->setActiveSlot(slot);
  (void)warmPool->preloadAll();

  (void)waitForWarmIdle(timeoutMs);

  if (warmPool->isWarm(slot)) return true;

  auto* instance = warmPool->acquire(slot);

  if (instance == nullptr) return false;

  bindVoice(slot, instance);
  warmPool->release(slot);
  return true;
}

bool WarmPoolController::waitForWarmIdle(int timeoutMs) {
  if (warmPool == nullptr) return true;

  const double deadline = juce::Time::getMillisecondCounterHiRes() +
                          static_cast<double>(std::max(0, timeoutMs));

  for (;;) {
    if (warmPool->isIdle()) return true;

    if (juce::Time::getMillisecondCounterHiRes() >= deadline) return false;

    pumpMessages(2);
  }
}

void WarmPoolController::releaseAllPinsLocked() {
  if (warmPool == nullptr) {
    pinnedActiveSlot = -1;
    pinnedPreviousSlot = -1;
    return;
  }

  const int previous = pinnedPreviousSlot;
  const int active = pinnedActiveSlot;

  pinnedPreviousSlot = -1;
  pinnedActiveSlot = -1;

  if (previous >= 0) warmPool->release(previous);

  if (active >= 0) warmPool->release(active);
}

void WarmPoolController::pumpMessages(int milliseconds) {
#if JUCE_MODAL_LOOPS_PERMITTED
  if (auto* manager = juce::MessageManager::getInstanceWithoutCreating();
      manager != nullptr && manager->isThisTheMessageThread()) {
    manager->runDispatchLoopUntil(milliseconds);
    return;
  }
#endif

  juce::Thread::sleep(milliseconds);
}

}  // namespace ihub::session
