#include "SandboxHost.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if JUCE_LINUX || JUCE_MAC
 #include <unistd.h>
#endif

namespace ihub::plugins
{
namespace
{
constexpr int minimumMidiChannel = 1;
constexpr int maximumMidiChannel = 16;
constexpr int maximumMidiNote = 127;

juce::uint8 channelStatus (juce::uint8 baseStatus, int channel) noexcept
{
    const auto clamped = juce::jlimit (minimumMidiChannel, maximumMidiChannel, channel);
    return static_cast<juce::uint8> (baseStatus | (clamped - 1));
}

std::int32_t currentProcessId() noexcept
{
   #if JUCE_LINUX || JUCE_MAC
    return static_cast<std::int32_t> (getpid());
   #else
    return 0;
   #endif
}

/** Creates a zero-length-backed file of exactly `size` bytes before mapping it.
    MemoryMappedFile requires the file to already exist at its final size. */
bool createSizedFile (const juce::File& file, juce::int64 size)
{
    file.deleteFile();

    juce::FileOutputStream stream (file);

    if (! stream.openedOk() || size <= 0)
        return false;

    if (! stream.setPosition (size - 1))
        return false;

    if (! stream.writeByte (0))
        return false;

    stream.flush();
    return true;
}
} // namespace

//==============================================================================
/**
    Polls the child for death and for a plugin that has stopped answering.

    `isRunning()` is a `waitpid`, so it cannot run on the audio thread. This
    thread owns that check; `process()` only reads the atomics the watchdog and
    the client publish.
*/
class SandboxHost::Watchdog final : public juce::Thread
{
public:
    Watchdog (SandboxHost& hostToWatch, int timeoutMs)
        : juce::Thread ("ihub-sandbox-watchdog"),
          host (hostToWatch),
          hangTimeoutMs (timeoutMs)
    {
    }

    void run() override
    {
        auto* shared = host.shared;

        if (shared == nullptr)
            return;

        auto lastHeartbeat = sandbox::loadRelaxed (shared->heartbeat);
        auto lastProgressMs = juce::Time::getMillisecondCounterHiRes();

        while (! threadShouldExit())
        {
            wait (20);

            if (threadShouldExit() || host.shared == nullptr || host.childProcess == nullptr)
                return;

            if (! host.childProcess->isRunning())
            {
                host.markFailed ("sandbox process exited unexpectedly");
                return;
            }

            const auto now = juce::Time::getMillisecondCounterHiRes();
            const auto heartbeat = sandbox::loadRelaxed (host.shared->heartbeat);

            if (heartbeat != lastHeartbeat)
            {
                lastHeartbeat = heartbeat;
                lastProgressMs = now;
                continue;
            }

            const auto outstanding = sandbox::loadAcquire (host.shared->requestPublished)
                                   - sandbox::loadAcquire (host.shared->requestConsumed);

            if (outstanding > 0 && now - lastProgressMs > static_cast<double> (hangTimeoutMs))
            {
                host.childProcess->kill();
                host.markFailed ("sandbox stopped responding");
                return;
            }
        }
    }

private:
    SandboxHost& host;
    const int hangTimeoutMs;

    JUCE_DECLARE_NON_COPYABLE (Watchdog)
};

//==============================================================================
SandboxHost::SandboxHost()
    : SandboxHost (Options())
{
}

SandboxHost::SandboxHost (Options optionsToUse)
    : options (std::move (optionsToUse))
{
    if (options.maxBlockSize <= 0)
        options.maxBlockSize = 512;

    if (options.numChannels <= 0)
        options.numChannels = 2;

    if (options.sampleRate <= 0.0)
        options.sampleRate = 48000.0;

    if (options.startupTimeoutMs <= 0)
        options.startupTimeoutMs = 15000;

    if (options.hangTimeoutMs <= 0)
        options.hangTimeoutMs = 2500;
}

SandboxHost::~SandboxHost()
{
    stop();
}

//==============================================================================
juce::File SandboxHost::findHelperExecutable()
{
    if (auto* overridden = std::getenv ("IHUB_SANDBOX"))
        if (*overridden != 0)
            return juce::File (juce::String::fromUTF8 (overridden)).getFullPathName();

   #if JUCE_WINDOWS
    const auto suffix = juce::String (".exe");
   #else
    const auto suffix = juce::String();
   #endif

    const auto appDirectory = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                  .getParentDirectory();

    return appDirectory.getChildFile (juce::String (sandbox::cli::helperName) + suffix);
}

//==============================================================================
void SandboxHost::resetTransportState() noexcept
{
    repliesPlayed = 0;

    droppedMidiEvents.store (0, std::memory_order_relaxed);
    underruns.store (0, std::memory_order_relaxed);
    droppedRequests.store (0, std::memory_order_relaxed);
    playedBlocks.store (0, std::memory_order_relaxed);
    pluginLatency.store (0, std::memory_order_relaxed);

    pendingReadIndex.store (0, std::memory_order_relaxed);
    pendingWriteIndex.store (0, std::memory_order_relaxed);
}

void SandboxHost::closeSharedMemory() noexcept
{
    mapping.reset();
    shared = nullptr;

    if (sharedMemoryFile != juce::File())
    {
        sharedMemoryFile.deleteFile();
        sharedMemoryFile = juce::File();
    }
}

juce::String SandboxHost::readChildError() const
{
    if (shared == nullptr)
        return {};

    return juce::String::fromUTF8 (shared->error).trim();
}

void SandboxHost::failStart (const juce::String& reason, juce::String& error)
{
    error = reason;

    if (watchdog != nullptr)
    {
        watchdog->signalThreadShouldExit();
        watchdog->notify();
        watchdog->waitForThreadToExit (2000);
        watchdog.reset();
    }

    if (shared != nullptr)
        sandbox::storeRelease (shared->state,
                               static_cast<std::int32_t> (sandbox::ProcessState::stopping));

    if (childProcess != nullptr)
    {
        if (childProcess->isRunning())
        {
            childProcess->waitForProcessToFinish (300);

            if (childProcess->isRunning())
            {
                childProcess->kill();
                childProcess->waitForProcessToFinish (1000);
            }
        }

        childProcess.reset();
    }

    closeSharedMemory();

    {
        const juce::ScopedLock lock (failureLock);
        failureReason = reason;
    }

    failed.store (true, std::memory_order_relaxed);
    status.store (Status::failed, std::memory_order_relaxed);

    std::function<void (Status, const juce::String&)> callback;

    {
        const juce::ScopedLock lock (failureLock);
        callback = statusCallback;
    }

    if (callback != nullptr)
        callback (Status::failed, reason);
}

bool SandboxHost::start (juce::String& error)
{
    error.clear();

    if (isRunning())
        return true;

    stop();
    resetTransportState();

    {
        const juce::ScopedLock lock (failureLock);
        failureReason.clear();
    }

    failed.store (false, std::memory_order_relaxed);
    status.store (Status::starting, std::memory_order_relaxed);

    const auto fail = [this, &error] (const juce::String& reason)
    {
        failStart (reason, error);
        return false;
    };

    if (options.sampleRate <= 0.0)
        return fail ("sandbox sample rate must be positive");

    if (options.maxBlockSize <= 0 || options.maxBlockSize > sandbox::maximumBlockSize)
        return fail ("sandbox block size is outside the shared-memory capacity (1.."
                     + juce::String (sandbox::maximumBlockSize) + ")");

    if (options.numChannels <= 0 || options.numChannels > sandbox::maximumChannels)
        return fail ("sandbox channel count is outside the shared-memory capacity (1.."
                     + juce::String (sandbox::maximumChannels) + ")");

    const bool hasPlugin = options.pluginBundle != juce::File();
    const bool hasFakePlugin = options.extraArguments.contains (juce::String (sandbox::cli::fakeToneFlag));

    if (! hasPlugin && ! hasFakePlugin)
        return fail ("no plugin bundle and no fake plugin requested");

    if (hasPlugin && ! options.pluginBundle.exists())
        return fail ("plugin bundle does not exist: " + options.pluginBundle.getFullPathName());

    const auto helper = options.helperExecutable != juce::File()
                            ? options.helperExecutable
                            : findHelperExecutable();

    if (! helper.existsAsFile())
        return fail ("sandbox helper not found: " + helper.getFullPathName());

    sharedMemoryFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("ihub-sandbox-" + juce::Uuid().toString() + ".ihubshm");

    if (! createSizedFile (sharedMemoryFile, static_cast<juce::int64> (sizeof (sandbox::SharedMemory))))
        return fail ("could not create the sandbox shared-memory file: "
                     + sharedMemoryFile.getFullPathName());

    mapping = std::make_unique<juce::MemoryMappedFile> (sharedMemoryFile,
                                                        juce::MemoryMappedFile::readWrite,
                                                        false);

    if (mapping->getData() == nullptr || mapping->getSize() < sizeof (sandbox::SharedMemory))
        return fail ("could not map the sandbox shared-memory file");

    shared = static_cast<sandbox::SharedMemory*> (mapping->getData());
    std::memset (shared, 0, sizeof (sandbox::SharedMemory));

    shared->magic = sandbox::sharedMemoryMagic;
    shared->version = sandbox::sharedMemoryVersion;
    shared->sampleRate = options.sampleRate;
    shared->maximumBlockSize = options.maxBlockSize;
    shared->numChannels = options.numChannels;
    shared->state = static_cast<std::int32_t> (sandbox::ProcessState::empty);
    shared->parentProcessId = currentProcessId();
    shared->latencySamples = -1;

    juce::StringArray arguments;
    arguments.add (helper.getFullPathName());
    arguments.add (sandbox::cli::mapFlag);
    arguments.add (sharedMemoryFile.getFullPathName());

    if (hasPlugin)
    {
        arguments.add (sandbox::cli::pluginFlag);
        arguments.add (options.pluginBundle.getFullPathName());
    }

    arguments.addArray (options.extraArguments);

    childProcess = std::make_unique<juce::ChildProcess>();

    if (! childProcess->start (arguments,
                               juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return fail ("could not launch sandbox helper: " + helper.getFullPathName());

    const auto deadline = juce::Time::getMillisecondCounterHiRes()
                        + static_cast<double> (options.startupTimeoutMs);

    for (;;)
    {
        const auto state = static_cast<sandbox::ProcessState> (
            sandbox::loadAcquire (shared->state));

        if (state == sandbox::ProcessState::ready)
            break;

        if (state == sandbox::ProcessState::failed)
        {
            auto reason = readChildError();

            if (reason.isEmpty())
                reason = "sandbox failed while loading the plugin";

            return fail (reason);
        }

        if (childProcess == nullptr || ! childProcess->isRunning())
        {
            auto reason = readChildError();

            if (reason.isEmpty())
                reason = "sandbox process exited during startup";

            return fail (reason);
        }

        if (juce::Time::getMillisecondCounterHiRes() > deadline)
        {
            childProcess->kill();
            return fail ("sandbox startup timed out after "
                         + juce::String (options.startupTimeoutMs) + " ms");
        }

        juce::Thread::sleep (2);
    }

    const auto reportedLatency = sandbox::loadRelaxed (shared->latencySamples);
    pluginLatency.store (juce::jmax (0, reportedLatency), std::memory_order_relaxed);

    status.store (Status::running, std::memory_order_relaxed);

    watchdog = std::make_unique<Watchdog> (*this, options.hangTimeoutMs);
    watchdog->startThread();

    return true;
}

void SandboxHost::stop()
{
    if (watchdog != nullptr)
    {
        watchdog->signalThreadShouldExit();
        watchdog->notify();
        watchdog->waitForThreadToExit (2000);
        watchdog.reset();
    }

    if (shared != nullptr)
        sandbox::storeRelease (shared->state,
                               static_cast<std::int32_t> (sandbox::ProcessState::stopping));

    if (childProcess != nullptr)
    {
        if (childProcess->isRunning())
        {
            childProcess->waitForProcessToFinish (300);

            if (childProcess->isRunning())
            {
                childProcess->kill();
                childProcess->waitForProcessToFinish (1000);
            }
        }

        childProcess->readAllProcessOutput();
        childProcess.reset();
    }

    closeSharedMemory();

    failed.store (false, std::memory_order_relaxed);
    status.store (Status::stopped, std::memory_order_relaxed);
}

bool SandboxHost::isRunning() const noexcept
{
    return status.load (std::memory_order_relaxed) == Status::running
        && ! failed.load (std::memory_order_relaxed);
}

//==============================================================================
SandboxHost::Status SandboxHost::getStatus() const noexcept
{
    return status.load (std::memory_order_relaxed);
}

bool SandboxHost::isFailed() const noexcept
{
    return failed.load (std::memory_order_relaxed);
}

juce::String SandboxHost::getFailureReason() const
{
    const juce::ScopedLock lock (failureLock);
    return failureReason;
}

void SandboxHost::setStatusCallback (std::function<void (Status, const juce::String&)> callback)
{
    const juce::ScopedLock lock (failureLock);
    statusCallback = std::move (callback);
}

void SandboxHost::markFailed (const juce::String& reason)
{
    // The first failure wins; a later message must not overwrite the cause.
    bool expected = false;

    if (! failed.compare_exchange_strong (expected, true, std::memory_order_relaxed))
        return;

    std::function<void (Status, const juce::String&)> callback;

    {
        const juce::ScopedLock lock (failureLock);
        failureReason = reason;
        callback = statusCallback;
    }

    status.store (Status::failed, std::memory_order_relaxed);

    if (callback != nullptr)
        callback (Status::failed, reason);
}

//==============================================================================
void SandboxHost::prepare (double sampleRate, int maximumBlockSize)
{
    preparedBlockSize.store (juce::jmax (1, maximumBlockSize), std::memory_order_relaxed);

    if (status.load (std::memory_order_relaxed) != Status::running)
        return;

    if (std::abs (sampleRate - options.sampleRate) > 0.5
        || maximumBlockSize <= 0
        || maximumBlockSize > options.maxBlockSize)
    {
        markFailed ("engine prepared the sandbox with an incompatible sample rate or block size");
    }
}

void SandboxHost::noteOn (int ch, int note, float vel)
{
    if (vel <= 0.0f)
    {
        noteOff (ch, note);
        return;
    }

    const auto velocity = static_cast<std::uint8_t> (juce::jlimit (
        1, 127, static_cast<int> (std::lround (vel * 127.0f))));

    const std::uint8_t message[3] {
        channelStatus (0x90, ch),
        static_cast<std::uint8_t> (juce::jlimit (0, maximumMidiNote, note)),
        velocity
    };

    pushEvent (message, 3);
}

void SandboxHost::noteOff (int ch, int note)
{
    const std::uint8_t message[3] {
        channelStatus (0x80, ch),
        static_cast<std::uint8_t> (juce::jlimit (0, maximumMidiNote, note)),
        0
    };

    pushEvent (message, 3);
}

bool SandboxHost::pushEvent (const std::uint8_t* bytes, int numBytes) noexcept
{
    if (bytes == nullptr || numBytes <= 0 || numBytes > 3)
        return false;

    const auto write = pendingWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1) % maxPendingMidiEvents;

    if (next == pendingReadIndex.load (std::memory_order_acquire))
    {
        droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    auto& event = pendingMidi[write];

    for (int i = 0; i < numBytes; ++i)
        event.bytes[i] = bytes[i];

    for (int i = numBytes; i < 3; ++i)
        event.bytes[i] = 0;

    event.numBytes = static_cast<std::uint8_t> (numBytes);

    pendingWriteIndex.store (next, std::memory_order_release);
    return true;
}

//==============================================================================
void SandboxHost::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    buffer.clear();

    if (shared == nullptr
        || status.load (std::memory_order_relaxed) != Status::running
        || failed.load (std::memory_order_relaxed))
    {
        return;
    }

    const int blockSamples = juce::jmin (numSamples, options.maxBlockSize);

    if (blockSamples <= 0)
        return;

    // --- Request: this block's size and MIDI --------------------------------
    const auto published = sandbox::loadAcquire (shared->requestPublished);
    const auto consumed = sandbox::loadAcquire (shared->requestConsumed);

    if (published - consumed < static_cast<std::uint64_t> (sandbox::requestRingSize))
    {
        auto& request = shared->requests[published % sandbox::requestRingSize];

        request.sequence = static_cast<std::uint32_t> (published);
        request.numSamples = blockSamples;

        std::uint32_t eventCount = 0;

        const auto addEvent = [&request, &eventCount, blockSamples] (const std::uint8_t* bytes,
                                                                     int numBytes,
                                                                     int sampleOffset) noexcept
        {
            if (bytes == nullptr || numBytes <= 0)
                return false;

            if (eventCount >= static_cast<std::uint32_t> (sandbox::maximumMidiEvents))
                return false;

            auto& event = request.midi[eventCount++];
            const auto bytesToCopy = juce::jmin (numBytes, 3);

            for (int i = 0; i < bytesToCopy; ++i)
                event.bytes[i] = bytes[i];

            for (int i = bytesToCopy; i < 3; ++i)
                event.bytes[i] = 0;

            event.numBytes = static_cast<std::uint8_t> (bytesToCopy);
            event.sampleOffset = juce::jlimit (0, juce::jmax (0, blockSamples - 1), sampleOffset);

            return true;
        };

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (! addEvent (message.getRawData(), message.getRawDataSize(),
                            metadata.samplePosition))
            {
                droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);
            }
        }

        auto read = pendingReadIndex.load (std::memory_order_relaxed);
        const auto write = pendingWriteIndex.load (std::memory_order_acquire);

        while (read != write)
        {
            const auto& event = pendingMidi[read];

            if (! addEvent (event.bytes, event.numBytes, 0))
                droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);

            read = (read + 1) % maxPendingMidiEvents;
        }

        pendingReadIndex.store (read, std::memory_order_release);

        request.numMidiEvents = eventCount;

        sandbox::storeRelease (shared->requestPublished, published + 1);
    }
    else
    {
        droppedRequests.fetch_add (1, std::memory_order_relaxed);
    }

    // --- Reply: the oldest block the sandbox has finished -------------------
    auto repliesAvailable = sandbox::loadAcquire (shared->replyPublished);

    // If the audio callback stalled long enough for the child to wrap the
    // reply ring, skip the overwritten blocks instead of replaying stale ones.
    if (repliesAvailable > repliesPlayed
        && repliesAvailable - repliesPlayed > static_cast<std::uint64_t> (sandbox::replyRingSize))
    {
        const auto skipped = repliesAvailable - repliesPlayed
                           - static_cast<std::uint64_t> (sandbox::replyRingSize);

        underruns.fetch_add (static_cast<int> (juce::jmin<std::uint64_t> (skipped, 1000000)),
                             std::memory_order_relaxed);
        repliesPlayed = repliesAvailable - static_cast<std::uint64_t> (sandbox::replyRingSize);
    }

    if (repliesAvailable > repliesPlayed)
    {
        const auto& reply = shared->replies[repliesPlayed % sandbox::replyRingSize];

        if (reply.sequence == static_cast<std::uint32_t> (repliesPlayed))
        {
            const auto samples = juce::jlimit (0, numSamples, reply.numSamples);
            const auto channels = juce::jmin (buffer.getNumChannels(), sandbox::maximumChannels);

            for (int channel = 0; channel < channels; ++channel)
                buffer.copyFrom (channel, 0, reply.audio[channel], samples);

            if (reply.latencySamples >= 0)
                pluginLatency.store (reply.latencySamples, std::memory_order_relaxed);

            playedBlocks.fetch_add (1, std::memory_order_relaxed);
        }
        else
        {
            underruns.fetch_add (1, std::memory_order_relaxed);
        }

        ++repliesPlayed;
    }
    else
    {
        underruns.fetch_add (1, std::memory_order_relaxed);
    }
}

int SandboxHost::latencySamples() const noexcept
{
    const auto plugin = juce::jmax (0, pluginLatency.load (std::memory_order_relaxed));

    if (! isRunning())
        return plugin;

    // The host pipelines one engine block: the block requested now is heard on
    // the next callback.
    const auto block = preparedBlockSize.load (std::memory_order_relaxed);
    return plugin + (block > 0 ? block : options.maxBlockSize);
}

int SandboxHost::getPluginLatencySamples() const noexcept
{
    return juce::jmax (0, pluginLatency.load (std::memory_order_relaxed));
}

int SandboxHost::getUnderrunCount() const noexcept
{
    return underruns.load (std::memory_order_relaxed);
}

int SandboxHost::getDroppedRequestCount() const noexcept
{
    return droppedRequests.load (std::memory_order_relaxed);
}

int SandboxHost::getDroppedMidiEventCount() const noexcept
{
    return droppedMidiEvents.load (std::memory_order_relaxed);
}

std::int64_t SandboxHost::getPlayedBlockCount() const noexcept
{
    return playedBlocks.load (std::memory_order_relaxed);
}

//==============================================================================
const char* toString (SandboxHost::Status status) noexcept
{
    switch (status)
    {
        case SandboxHost::Status::stopped:  return "stopped";
        case SandboxHost::Status::starting: return "starting";
        case SandboxHost::Status::running:  return "running";
        case SandboxHost::Status::failed:   return "failed";
    }

    return "failed";
}

} // namespace ihub::plugins
