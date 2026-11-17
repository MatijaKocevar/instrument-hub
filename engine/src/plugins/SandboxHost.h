#pragma once

#include "SandboxClient.h"
#include "SandboxSlotControl.h"

#include "../audio/Slot.h"
#include "../instruments/Instrument.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ihub::plugins {
class SandboxWatchdog;

class SandboxHost final : public Instrument,
                          public AudioEngine::SlotSource,
                          public SandboxSlotControl {
public:
    enum class Status { stopped, starting, running, failed };

    struct Options {
        juce::File pluginBundle;

        juce::StringArray extraArguments;

        juce::File helperExecutable;

        double sampleRate = 48000.0;
        int maxBlockSize = 512;
        int numChannels = 2;

        int slotIndex = -1;
        std::uint64_t generation = 0;

        int startupTimeoutMs = 15000;

        int hangTimeoutMs = 5000;

        std::function<bool()> shouldAbortStart;
    };

    SandboxHost();
    explicit SandboxHost(Options options);
    ~SandboxHost() override;

    bool start(juce::String& error);

    void stop();

    bool isRunning() const noexcept override;

    Status getStatus() const noexcept;
    bool isFailed() const noexcept override;

    juce::String getFailureReason() const override;

    void setStatusCallback(std::function<void(Status, const juce::String&)> callback);

    bool setStateInformation(const void* data, int numBytes) override;
    bool setStateInformation(const juce::MemoryBlock& block) override;
    juce::MemoryBlock getStateInformation() override;

    using StateChangedCallback =
        std::function<void(const juce::MemoryBlock&, std::uint64_t revision)>;

    void setStateChangedCallback(StateChangedCallback callback);

    juce::MemoryBlock getLastCommittedState() const override;
    std::uint64_t getLastCommittedStateRevision() const noexcept override;

    bool flushPublishedState() override;

    using ParameterEdit = SandboxParameterEdit;

    bool setParameters(const std::vector<ParameterEdit>& edits) override;

    bool openEditor(juce::String& error) override;
    bool closeEditor(juce::String& error) override;
    bool isEditorOpen() const noexcept override;

    int getChildProcessId() const noexcept override;

    int getSlotIndex() const noexcept { return options.slotIndex; }
    std::uint64_t getGeneration() const noexcept { return options.generation; }

    void prepare(double sampleRate, int maximumBlockSize) override;

    void noteOn(int ch, int note, float vel) override;
    void noteOff(int ch, int note) override;

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    int latencySamples() const noexcept override;

    Instrument::Kind kind() const noexcept override { return Instrument::Kind::Plugin; }

    int getPluginLatencySamples() const noexcept override;
    int getUnderrunCount() const noexcept override;
    int getDroppedRequestCount() const noexcept override;
    int getDroppedMidiEventCount() const noexcept override;
    std::int64_t getPlayedBlockCount() const noexcept override;

    juce::File getSharedMemoryFile() const noexcept { return sharedMemoryFile; }

    const Options& getOptions() const noexcept { return options; }

    static juce::File findHelperExecutable();

private:
    friend class SandboxWatchdog;

    struct PendingMidiEvent {
        std::uint8_t bytes[3]{0, 0, 0};
        std::uint8_t numBytes = 0;
    };

    static constexpr int maxPendingMidiEvents = 256;

    bool pushEvent(const std::uint8_t* bytes, int numBytes) noexcept;

    void markFailed(const juce::String& reason);
    void failStart(const juce::String& reason, juce::String& error);
    void resetTransportState() noexcept;
    void closeSharedMemory() noexcept;
    juce::String readChildError() const;
    juce::String readControlError() const;
    void commitState(const juce::MemoryBlock& state, bool notify);
    bool runControlCommand(sandbox::ControlCommand command, const juce::MemoryBlock& input,
                           juce::MemoryBlock& output, int timeoutMs);
    sandbox::SharedMemory* getShared() const noexcept { return shared; }

    Options options;

    juce::File sharedMemoryFile;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    sandbox::SharedMemory* shared = nullptr;

    std::unique_ptr<juce::ChildProcess> childProcess;
    std::unique_ptr<SandboxWatchdog> watchdog;

    juce::CriticalSection controlLock;
    juce::File stateTransferFile;
    juce::File editorStateFile;

    mutable juce::CriticalSection stateLock;
    juce::MemoryBlock lastCommittedState;
    std::uint64_t lastCommittedRevision = 0;
    std::uint32_t seenEditorStateRevision = 0;
    StateChangedCallback stateChangedCallback;

    std::atomic<Status> status{Status::stopped};
    std::atomic<bool> failed{false};
    std::atomic<int> pluginLatency{0};
    std::atomic<int> preparedBlockSize{0};

    mutable juce::CriticalSection failureLock;
    juce::String failureReason;
    std::function<void(Status, const juce::String&)> statusCallback;

    std::array<PendingMidiEvent, maxPendingMidiEvents> pendingMidi{};
    std::atomic<std::uint32_t> pendingWriteIndex{0};
    std::atomic<std::uint32_t> pendingReadIndex{0};

    std::atomic<int> droppedMidiEvents{0};
    std::atomic<int> underruns{0};
    std::atomic<int> droppedRequests{0};
    std::atomic<std::int64_t> playedBlocks{0};

    std::uint64_t repliesPlayed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SandboxHost)
};

const char* toString(SandboxHost::Status status) noexcept;

}  // namespace ihub::plugins
