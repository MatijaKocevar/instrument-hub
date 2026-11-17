#include "../SandboxHost.h"

namespace ihub::plugins {
bool SandboxHost::isRunning() const noexcept {
  return status.load(std::memory_order_relaxed) == Status::running &&
         !failed.load(std::memory_order_relaxed);
}

SandboxHost::Status SandboxHost::getStatus() const noexcept {
  return status.load(std::memory_order_relaxed);
}

bool SandboxHost::isFailed() const noexcept {
  return failed.load(std::memory_order_relaxed);
}

juce::String SandboxHost::getFailureReason() const {
  const juce::ScopedLock lock(failureLock);
  return failureReason;
}

void SandboxHost::setStatusCallback(
    std::function<void(Status, const juce::String&)> callback) {
  const juce::ScopedLock lock(failureLock);
  statusCallback = std::move(callback);
}

void SandboxHost::markFailed(const juce::String& reason) {
  bool expected = false;

  if (!failed.compare_exchange_strong(expected, true,
                                      std::memory_order_relaxed))
    return;

  std::function<void(Status, const juce::String&)> callback;

  {
    const juce::ScopedLock lock(failureLock);
    failureReason = reason;
    callback = statusCallback;
  }

  status.store(Status::failed, std::memory_order_relaxed);

  if (callback != nullptr) callback(Status::failed, reason);
}

}  // namespace ihub::plugins
