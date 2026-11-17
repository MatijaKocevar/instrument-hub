#include "../SandboxHost.h"

#include <utility>

namespace ihub::plugins {

void SandboxHost::setStateChangedCallback(StateChangedCallback callback) {
  const juce::ScopedLock lock(stateLock);
  stateChangedCallback = std::move(callback);
}

juce::MemoryBlock SandboxHost::getLastCommittedState() const {
  const juce::ScopedLock lock(stateLock);
  return lastCommittedState;
}

std::uint64_t SandboxHost::getLastCommittedStateRevision() const noexcept {
  const juce::ScopedLock lock(stateLock);
  return lastCommittedRevision;
}

void SandboxHost::commitState(const juce::MemoryBlock& state, bool notify) {
  if (state.getSize() == 0) return;

  StateChangedCallback callback;

  std::uint64_t revision = 0;

  {
    const juce::ScopedLock lock(stateLock);

    lastCommittedState = state;
    revision = ++lastCommittedRevision;

    if (notify) callback = stateChangedCallback;
  }

  if (callback != nullptr) {
    try {
      callback(state, revision);
    } catch (...) {
    }
  }
}

bool SandboxHost::flushPublishedState() {
  if (shared == nullptr) return false;

  const auto published = sandbox::loadAcquire(shared->editorStateRevision);

  if (published == 0) return false;

  juce::File path;
  std::int32_t expectedBytes = 0;

  {
    const juce::ScopedLock lock(stateLock);

    if (static_cast<std::uint32_t>(published) == seenEditorStateRevision)
      return false;

    seenEditorStateRevision = static_cast<std::uint32_t>(published);
    expectedBytes = sandbox::loadRelaxed(shared->editorStateBytes);
    path = juce::File(juce::String::fromUTF8(shared->editorStatePath));
  }

  if (path == juce::File() || expectedBytes <= 0) return false;

  juce::MemoryBlock state;

  if (!path.existsAsFile() || !path.loadFileAsData(state)) return false;

  if (static_cast<std::int32_t>(state.getSize()) != expectedBytes) return false;

  commitState(state, true);
  return true;
}

}  // namespace ihub::plugins
