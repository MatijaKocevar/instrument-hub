#pragma once

#include "SandboxClient.h"

#include "../audio/Slot.h"
#include "../instruments/Instrument.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace ihub::plugins
{
class SandboxWatchdog;

/**
    Optional out-of-process hosting for one plugin instrument.

    `PluginInstrument` loads the plugin into the application process: fast, but
    a hostile or broken plugin can take the engine (and everything the user has
    not saved) down with it. `SandboxHost` trades that risk for one block of
    extra latency: the plugin lives in an `ihub-sandbox` child process, audio
    and MIDI cross through a pre-allocated shared-memory file, and the engine
    only ever sees this proxy.

    ## Audio transport

    Every `process()` call writes one request block (block size plus merged
    MIDI) into the request ring and plays the oldest reply from the reply ring.
    That pipeline is why the reported latency includes one block on top of the
    plugin's own latency. The callback never waits, never allocates, takes no
    locks and never logs: a missing reply becomes silence plus an underrun
    counter.

    ## Failure handling

    A watchdog thread polls the child process every few milliseconds. When the
    child dies (a plugin `abort()`, a segfault, or a test that triggers one),
    the watchdog:

    - sets `getStatus()` to `Status::failed`,
    - stores a human-readable reason for `getFailureReason()`,
    - invokes the status callback once, so the application can mark the slot,
    - leaves the shared mapping valid while `process()` continues to return
      silence.

    The rest of the graph is untouched: other slots keep rendering and the
    audio callback keeps its real-time guarantees. A hung plugin is treated the
    same way after `Options::hangTimeoutMs` without a heartbeat; the child is
    killed first.

    ## Threading

    - `start()`, `stop()`, `prepare()`, `getFailureReason()` and
      `setStatusCallback()` are message-thread calls.
    - `noteOn()`, `noteOff()` queue into a lock-free SPSC ring and are safe
      from any thread.
    - `process()` is audio-thread only (the Instrument/SlotSource contract).
    - `getStatus()`, `isFailed()` and the counters are safe from any thread.

    `stop()` requires the audio stream to be stopped, the same rule as
    `PluginInstrument::unload()`; it unmaps the shared file.
*/
class SandboxHost final : public Instrument,
                          public AudioEngine::SlotSource
{
public:
    enum class Status
    {
        stopped,
        starting,
        running,
        failed
    };

    struct Options
    {
        /** Plugin bundle to host. Empty means the caller is using a test seam
            such as `--fake-tone` in `extraArguments`. */
        juce::File pluginBundle;

        /** Extra command-line arguments for the helper. The acceptance test
            uses `--fake-tone <hz>` and `--abort-after <blocks>`. */
        juce::StringArray extraArguments;

        /** Path to ihub-sandbox. Empty means "find it next to the app". */
        juce::File helperExecutable;

        double sampleRate = 48000.0;
        int maxBlockSize = 512;
        int numChannels = 2;

        /** Time allowed for the child to load its plugin and signal ready. */
        int startupTimeoutMs = 15000;

        /** A running but unresponsive child is killed after this long. */
        int hangTimeoutMs = 2500;
    };

    SandboxHost();
    explicit SandboxHost (Options options);
    ~SandboxHost() override;

    //==============================================================================
    // Lifecycle (message thread)

    /** Creates the shared mapping, launches the helper and waits for it to
        load the plugin. On failure fills `error` and leaves the status
        `failed`; the host can be started again after `stop()`. */
    bool start (juce::String& error);

    /** Stops the child and releases the mapping. The audio stream must be
        stopped. Safe to call when nothing is running. */
    void stop();

    bool isRunning() const noexcept;

    //==============================================================================
    // Slot failure reporting

    Status getStatus() const noexcept;
    bool isFailed() const noexcept;

    /** Why the slot failed. Empty while healthy. Thread-safe. */
    juce::String getFailureReason() const;

    /** Installed once by the application; called on the watchdog thread the
        first time the slot fails, and again if it is restarted and fails. */
    void setStatusCallback (std::function<void (Status, const juce::String&)> callback);

    //==============================================================================
    // Instrument / SlotSource

    void prepare (double sampleRate, int maximumBlockSize) override;

    void noteOn (int ch, int note, float vel) override;
    void noteOff (int ch, int note) override;

    /** Renders whatever the sandbox last produced. Audio thread, real-time
        safe. When the host is stopped, failed or awaiting its first reply the
        buffer is cleared. */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /** Plugin latency plus one block of transport pipeline. */
    int latencySamples() const noexcept override;

    Instrument::Kind kind() const noexcept override { return Instrument::Kind::Plugin; }

    //==============================================================================
    // Diagnostics

    int getPluginLatencySamples() const noexcept;
    int getUnderrunCount() const noexcept;
    int getDroppedRequestCount() const noexcept;
    int getDroppedMidiEventCount() const noexcept;
    std::int64_t getPlayedBlockCount() const noexcept;

    juce::File getSharedMemoryFile() const noexcept { return sharedMemoryFile; }

    const Options& getOptions() const noexcept { return options; }

    /** Locate the helper: IHUB_SANDBOX overrides, otherwise the ihub-sandbox
        sibling of the current executable (with .exe on Windows). */
    static juce::File findHelperExecutable();

private:
    friend class SandboxWatchdog;

    struct PendingMidiEvent
    {
        std::uint8_t bytes[3] { 0, 0, 0 };
        std::uint8_t numBytes = 0;
    };

    static constexpr int maxPendingMidiEvents = 256;

    bool pushEvent (const std::uint8_t* bytes, int numBytes) noexcept;

    void markFailed (const juce::String& reason);
    void failStart (const juce::String& reason, juce::String& error);
    void resetTransportState() noexcept;
    void closeSharedMemory() noexcept;
    juce::String readChildError() const;
    sandbox::SharedMemory* getShared() const noexcept { return shared; }

    Options options;

    juce::File sharedMemoryFile;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    sandbox::SharedMemory* shared = nullptr;

    std::unique_ptr<juce::ChildProcess> childProcess;
    std::unique_ptr<SandboxWatchdog> watchdog;

    std::atomic<Status> status { Status::stopped };
    std::atomic<bool> failed { false };
    std::atomic<int> pluginLatency { 0 };
    std::atomic<int> preparedBlockSize { 0 };

    mutable juce::CriticalSection failureLock;
    juce::String failureReason;
    std::function<void (Status, const juce::String&)> statusCallback;

    // Lock-free note queue, same shape as PluginInstrument's.
    std::array<PendingMidiEvent, maxPendingMidiEvents> pendingMidi {};
    std::atomic<std::uint32_t> pendingWriteIndex { 0 };
    std::atomic<std::uint32_t> pendingReadIndex { 0 };

    std::atomic<int> droppedMidiEvents { 0 };
    std::atomic<int> underruns { 0 };
    std::atomic<int> droppedRequests { 0 };
    std::atomic<std::int64_t> playedBlocks { 0 };

    // Audio-thread-owned pipeline cursors.
    std::uint64_t repliesPlayed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandboxHost)
};

/** Stable text for logs, tests and the control API. */
const char* toString (SandboxHost::Status status) noexcept;

} // namespace ihub::plugins
