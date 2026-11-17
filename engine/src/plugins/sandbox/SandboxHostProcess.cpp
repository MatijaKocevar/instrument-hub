#include "../SandboxHost.h"

#include "SandboxWatchdog.h"

#include <cstdio>
#include <cstring>

#if JUCE_LINUX || JUCE_MAC
#include <unistd.h>
#endif

namespace ihub::plugins {
namespace {
std::int32_t currentProcessId() noexcept {
#if JUCE_LINUX || JUCE_MAC
  return static_cast<std::int32_t>(getpid());
#else
  return 0;
#endif
}

bool createSizedFile(const juce::File& file, juce::int64 size) {
  file.deleteFile();

  juce::FileOutputStream stream(file);

  if (!stream.openedOk() || size <= 0) return false;

  if (!stream.setPosition(size - 1)) return false;

  if (!stream.writeByte(0)) return false;

  stream.flush();
  return true;
}
}  // namespace

void SandboxHost::resetTransportState() noexcept {
  repliesPlayed = 0;

  droppedMidiEvents.store(0, std::memory_order_relaxed);
  underruns.store(0, std::memory_order_relaxed);
  droppedRequests.store(0, std::memory_order_relaxed);
  playedBlocks.store(0, std::memory_order_relaxed);
  pluginLatency.store(0, std::memory_order_relaxed);

  pendingReadIndex.store(0, std::memory_order_relaxed);
  pendingWriteIndex.store(0, std::memory_order_relaxed);
}

juce::String SandboxHost::readChildError() const {
  if (shared == nullptr) return {};

  return juce::String::fromUTF8(shared->error).trim();
}

void SandboxHost::failStart(const juce::String& reason, juce::String& error) {
  error = reason;

  if (watchdog != nullptr) {
    watchdog->signalThreadShouldExit();
    watchdog->notify();
    watchdog->waitForThreadToExit(2000);
    watchdog.reset();
  }

  if (shared != nullptr)
    sandbox::storeRelease(shared->state, static_cast<std::int32_t>(
                                             sandbox::ProcessState::stopping));

  if (childProcess != nullptr) {
    if (childProcess->isRunning()) {
      childProcess->waitForProcessToFinish(300);

      if (childProcess->isRunning()) {
        childProcess->kill();
        childProcess->waitForProcessToFinish(1000);
      }
    }

    childProcess.reset();
  }

  closeSharedMemory();

  {
    const juce::ScopedLock lock(failureLock);
    failureReason = reason;
  }

  failed.store(true, std::memory_order_relaxed);
  status.store(Status::failed, std::memory_order_relaxed);

  std::function<void(Status, const juce::String&)> callback;

  {
    const juce::ScopedLock lock(failureLock);
    callback = statusCallback;
  }

  if (callback != nullptr) callback(Status::failed, reason);
}

bool SandboxHost::start(juce::String& error) {
  error.clear();

  if (isRunning()) return true;

  stop();
  resetTransportState();

  {
    const juce::ScopedLock lock(failureLock);
    failureReason.clear();
  }

  failed.store(false, std::memory_order_relaxed);
  status.store(Status::starting, std::memory_order_relaxed);

  const auto fail = [this, &error](const juce::String& reason) {
    failStart(reason, error);
    return false;
  };

  if (options.shouldAbortStart && options.shouldAbortStart())
    return fail("sandbox startup cancelled");

  if (options.sampleRate <= 0.0)
    return fail("sandbox sample rate must be positive");

  if (options.maxBlockSize <= 0 ||
      options.maxBlockSize > sandbox::maximumBlockSize)
    return fail(
        "sandbox block size is outside the shared-memory capacity (1.." +
        juce::String(sandbox::maximumBlockSize) + ")");

  if (options.numChannels <= 0 ||
      options.numChannels > sandbox::maximumChannels)
    return fail(
        "sandbox channel count is outside the shared-memory capacity (1.." +
        juce::String(sandbox::maximumChannels) + ")");

  const bool hasPlugin = options.pluginBundle != juce::File();
  const bool hasFakePlugin =
      options.extraArguments.contains(juce::String(sandbox::cli::fakeToneFlag));

  if (!hasPlugin && !hasFakePlugin)
    return fail("no plugin bundle and no fake plugin requested");

  if (hasPlugin && !options.pluginBundle.exists())
    return fail("plugin bundle does not exist: " +
                options.pluginBundle.getFullPathName());

  const auto helper = options.helperExecutable != juce::File()
                          ? options.helperExecutable
                          : findHelperExecutable();

  if (!helper.existsAsFile())
    return fail("sandbox helper not found: " + helper.getFullPathName());

  sharedMemoryFile =
      juce::File::getSpecialLocation(juce::File::tempDirectory)
          .getChildFile("ihub-sandbox-" + juce::Uuid().toString() + ".ihubshm");

  if (!createSizedFile(sharedMemoryFile,
                       static_cast<juce::int64>(sizeof(sandbox::SharedMemory))))
    return fail("could not create the sandbox shared-memory file: " +
                sharedMemoryFile.getFullPathName());

  mapping = std::make_unique<juce::MemoryMappedFile>(
      sharedMemoryFile, juce::MemoryMappedFile::readWrite, false);

  if (mapping->getData() == nullptr ||
      mapping->getSize() < sizeof(sandbox::SharedMemory))
    return fail("could not map the sandbox shared-memory file");

  shared = static_cast<sandbox::SharedMemory*>(mapping->getData());
  std::memset(shared, 0, sizeof(sandbox::SharedMemory));

  shared->magic = sandbox::sharedMemoryMagic;
  shared->version = sandbox::sharedMemoryVersion;
  shared->sampleRate = options.sampleRate;
  shared->maximumBlockSize = options.maxBlockSize;
  shared->numChannels = options.numChannels;
  shared->state = static_cast<std::int32_t>(sandbox::ProcessState::empty);
  shared->parentProcessId = currentProcessId();
  shared->latencySamples = -1;

  editorStateFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("ihub-sandbox-editor-" +
                                      juce::Uuid().toString() + ".bin");
  editorStateFile.deleteFile();

  std::snprintf(shared->editorStatePath, sizeof(shared->editorStatePath), "%s",
                editorStateFile.getFullPathName().toRawUTF8());

  {
    const juce::ScopedLock lock(stateLock);
    lastCommittedState.reset();
    lastCommittedRevision = 0;
    seenEditorStateRevision = 0;
  }

  juce::StringArray arguments;
  arguments.add(helper.getFullPathName());
  arguments.add(sandbox::cli::mapFlag);
  arguments.add(sharedMemoryFile.getFullPathName());

  if (hasPlugin) {
    arguments.add(sandbox::cli::pluginFlag);
    arguments.add(options.pluginBundle.getFullPathName());
  }

  arguments.addArray(options.extraArguments);

  childProcess = std::make_unique<juce::ChildProcess>();

  if (!childProcess->start(arguments, juce::ChildProcess::wantStdOut |
                                          juce::ChildProcess::wantStdErr))
    return fail("could not launch sandbox helper: " + helper.getFullPathName());

  const auto deadline = juce::Time::getMillisecondCounterHiRes() +
                        static_cast<double>(options.startupTimeoutMs);

  for (;;) {
    if (options.shouldAbortStart && options.shouldAbortStart()) {
      if (childProcess != nullptr) childProcess->kill();

      return fail("sandbox startup cancelled");
    }

    const auto state =
        static_cast<sandbox::ProcessState>(sandbox::loadAcquire(shared->state));

    if (state == sandbox::ProcessState::ready) break;

    if (state == sandbox::ProcessState::failed) {
      auto reason = readChildError();

      if (reason.isEmpty()) reason = "sandbox failed while loading the plugin";

      return fail(reason);
    }

    if (childProcess == nullptr || !childProcess->isRunning()) {
      auto reason = readChildError();

      if (reason.isEmpty()) reason = "sandbox process exited during startup";

      return fail(reason);
    }

    if (juce::Time::getMillisecondCounterHiRes() > deadline) {
      childProcess->kill();
      return fail("sandbox startup timed out after " +
                  juce::String(options.startupTimeoutMs) + " ms");
    }

    juce::Thread::sleep(2);
  }

  const auto reportedLatency = sandbox::loadRelaxed(shared->latencySamples);
  pluginLatency.store(juce::jmax(0, reportedLatency),
                      std::memory_order_relaxed);

  status.store(Status::running, std::memory_order_relaxed);

  watchdog = std::make_unique<SandboxWatchdog>(*this, options.hangTimeoutMs);
  watchdog->startThread();

  return true;
}

}  // namespace ihub::plugins
