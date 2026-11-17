#include "PageHost.h"

#include <algorithm>

namespace ihub::plugins {

const char* PageHost::toString(Status status) noexcept {
  switch (status) {
    case Status::stopped:
      return "stopped";
    case Status::starting:
      return "starting";
    case Status::ready:
      return "ready";
    case Status::recovering:
      return "recovering";
    case Status::failed:
      return "failed";
  }

  return "failed";
}

std::shared_ptr<PageHost> PageHost::create(Options optionsToUse) {
  return std::shared_ptr<PageHost>(new PageHost(std::move(optionsToUse)));
}

PageHost::PageHost(Options optionsToUse) : options(std::move(optionsToUse)) {
  if (options.sampleRate <= 0.0) options.sampleRate = 48000.0;

  if (options.maxBlockSize <= 0) options.maxBlockSize = 512;

  if (options.numChannels <= 0) options.numChannels = 2;

  if (options.maxBlockSize > sandbox::maximumBlockSize)
    options.maxBlockSize = sandbox::maximumBlockSize;

  if (options.numChannels > sandbox::maximumChannels)
    options.numChannels = sandbox::maximumChannels;

  if (options.startupTimeoutMs <= 0) options.startupTimeoutMs = 120000;

  if (options.hangTimeoutMs <= 0) options.hangTimeoutMs = 5000;

  if (options.controlTimeoutMs <= 0) options.controlTimeoutMs = 3000;

  if (options.recoveryStateTimeoutMs <= 0)
    options.recoveryStateTimeoutMs = 15000;

  if (options.recoveryAttempts <= 0) options.recoveryAttempts = 3;

  if (options.recoveryBackoffMs < 0) options.recoveryBackoffMs = 0;

  for (const auto& descriptor : options.slots) {
    if (descriptor.slotIndex < 0 ||
        descriptor.slotIndex >= static_cast<int>(sandbox::maximumPageSlots * 8))
      continue;

    if (std::find_if(slots.begin(), slots.end(),
                     [&descriptor](const std::unique_ptr<SlotState>& slot) {
                       return slot->descriptor.slotIndex ==
                              descriptor.slotIndex;
                     }) != slots.end())
      continue;

    auto slot = std::make_unique<SlotState>();
    slot->descriptor = descriptor;
    slot->internalIndex = static_cast<int>(slots.size());
    slots.push_back(std::move(slot));
  }

  for (const auto& slot : slots)
    internalToLogical.push_back(slot->descriptor.slotIndex);
}

PageHost::~PageHost() { shutdown(); }

PageHost::Status PageHost::getStatus() const noexcept {
  return status.load(std::memory_order_acquire);
}

bool PageHost::isReady() const noexcept { return getStatus() == Status::ready; }

bool PageHost::isFailed() const noexcept {
  return getStatus() == Status::failed;
}

juce::String PageHost::getFailureReason() const {
  const std::lock_guard<std::mutex> lock(statusLock);
  return failureReason;
}

void PageHost::setStatusCallback(StatusCallback callback) {
  const std::lock_guard<std::mutex> lock(statusLock);
  statusCallback = std::move(callback);
}

void PageHost::setSlotStateCallback(int slot, SlotStateCallback callback) {
  if (auto* state = findSlot(slot); state != nullptr) {
    const std::lock_guard<std::mutex> lock(state->stateLock);
    state->stateCallback = std::move(callback);
  }
}

void PageHost::setRecoveredCallback(RecoveredCallback callback) {
  const std::lock_guard<std::mutex> lock(statusLock);
  recoveredCallback = std::move(callback);
}

void PageHost::setActiveSlot(int slot) noexcept {
  activeSlot.store(slot, std::memory_order_relaxed);
}

int PageHost::getActiveSlot() const noexcept {
  return activeSlot.load(std::memory_order_relaxed);
}

std::vector<int> PageHost::getSlotIndices() const {
  std::vector<int> result;
  result.reserve(slots.size());

  for (const auto& slot : slots) result.push_back(slot->descriptor.slotIndex);

  return result;
}

bool PageHost::hasSlot(int slot) const noexcept {
  return findSlot(slot) != nullptr;
}

int PageHost::findInternalIndex(int slot) const noexcept {
  if (const auto* state = findSlot(slot); state != nullptr)
    return state->internalIndex;

  return -1;
}

int PageHost::getLogicalSlotForInternal(int internalIndex) const noexcept {
  if (internalIndex < 0 ||
      internalIndex >= static_cast<int>(internalToLogical.size()))
    return -1;

  return internalToLogical[static_cast<std::size_t>(internalIndex)];
}

int PageHost::getChildProcessId() const noexcept {
  return childProcessId.load(std::memory_order_relaxed);
}

int PageHost::getProcessId() const noexcept { return getChildProcessId(); }

PageHost::SlotState* PageHost::findSlot(int slot) noexcept {
  for (auto& state : slots)
    if (state->descriptor.slotIndex == slot) return state.get();

  return nullptr;
}

const PageHost::SlotState* PageHost::findSlot(int slot) const noexcept {
  for (const auto& state : slots)
    if (state->descriptor.slotIndex == slot) return state.get();

  return nullptr;
}

sandbox::SharedMemory* PageHost::channelFor(SlotState& slot) const noexcept {
  auto* page = sharedPage.load(std::memory_order_acquire);

  if (page == nullptr || slot.internalIndex < 0 ||
      slot.internalIndex >= page->numSlots)
    return nullptr;

  return &page->slots[slot.internalIndex];
}

sandbox::SharedMemory* PageHost::getSlotChannel(
    int internalIndex) const noexcept {
  auto* page = sharedPage.load(std::memory_order_acquire);

  if (page == nullptr || internalIndex < 0 || internalIndex >= page->numSlots)
    return nullptr;

  return &page->slots[internalIndex];
}

void PageHost::markStatus(Status newStatus, const juce::String& reason) {
  StatusCallback callback;
  Status previous = getStatus();

  {
    const std::lock_guard<std::mutex> lock(statusLock);

    if (newStatus == Status::ready || newStatus == Status::starting)
      failureReason.clear();
    else if (reason.isNotEmpty())
      failureReason = reason;

    callback = statusCallback;
    status.store(newStatus, std::memory_order_release);
  }

  if (callback != nullptr && previous != newStatus) {
    try {
      callback(newStatus, getFailureReason());
    } catch (...) {
    }
  }
}

PageHost::SlotStatus PageHost::getSlotStatus(int slot) const {
  SlotStatus result;
  result.slot = slot;

  const auto* state = findSlot(slot);

  if (state == nullptr) {
    result.failed = true;
    result.error = "the slot is not part of this plugin page";
    return result;
  }

  result.running = isReady() && !state->failed.load(std::memory_order_relaxed);
  result.failed = isFailed() || state->failed.load(std::memory_order_relaxed);
  result.latencySamples = state->latencySamples.load(std::memory_order_relaxed);

  if (result.failed) {
    {
      const std::lock_guard<std::mutex> lock(state->stateLock);
      result.error = state->localError;
    }

    if (result.error.isEmpty()) {
      if (auto* page = sharedPage.load(std::memory_order_acquire);
          page != nullptr && state->internalIndex >= 0 &&
          state->internalIndex < page->numSlots) {
        result.error =
            juce::String::fromUTF8(page->slots[state->internalIndex].error)
                .trim();
      }
    }

    if (result.error.isEmpty()) result.error = getFailureReason();
  }

  if (isReady())
    result.latencySamples += preparedBlockSize.load(std::memory_order_relaxed);

  return result;
}

bool PageHost::abortRequested() const noexcept {
  const std::lock_guard<std::mutex> lock(recoveryMutex);
  return shuttingDown;
}

void PageHost::closeRetiredMappings() noexcept { retiredMappings.clear(); }

}  // namespace ihub::plugins
