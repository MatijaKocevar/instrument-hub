#pragma once

#include "SandboxClient.h"
#include "SandboxSlotControl.h"

#include "../audio/Slot.h"
#include "../instruments/Instrument.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ihub::plugins
{

struct PageSlotDescriptor
{
    int slotIndex = -1;
    juce::File pluginBundle;
    juce::StringArray extraArguments;
    double fakeToneFrequency = 0.0;
    int abortAfterBlocks = -1;
};

class PageHostSlot;

class PageHost final : public std::enable_shared_from_this<PageHost>
{
public:
    enum class Status
    {
        stopped,
        starting,
        ready,
        recovering,
        failed
    };

    struct Options
    {
        juce::String pageId = "tracks";
        std::vector<PageSlotDescriptor> slots;
        juce::File helperExecutable;
        juce::File tempDirectory;

        double sampleRate = 48000.0;
        int maxBlockSize = 512;
        int numChannels = 2;

        int startupTimeoutMs = 120000;
        int hangTimeoutMs = 5000;
        int controlTimeoutMs = 3000;
        int recoveryCaptureTimeoutMs = 500;
        int recoveryStateTimeoutMs = 15000;
        int recoveryAttempts = 3;
        int recoveryBackoffMs = 500;

        std::function<bool()> shouldAbortStart;
    };

    struct SlotStatus
    {
        int slot = -1;
        bool running = false;
        bool failed = false;
        int latencySamples = 0;
        juce::String error;
    };

    using StatusCallback = std::function<void (Status, const juce::String&)>;
    using SlotStateCallback = std::function<void (int slot,
                                                  const juce::MemoryBlock& state,
                                                  std::uint64_t revision)>;
    using RecoveredCallback = std::function<void (int activeSlot)>;

    static std::shared_ptr<PageHost> create (Options options);
    ~PageHost();

    bool start (juce::String& error);
    void shutdown() noexcept;

    Status getStatus() const noexcept;
    bool isReady() const noexcept;
    bool isFailed() const noexcept;
    juce::String getFailureReason() const;
    juce::String getPageId() const { return options.pageId; }

    std::vector<int> getSlotIndices() const;
    bool hasSlot (int slot) const noexcept;
    int findInternalIndex (int slot) const noexcept;
    int getLogicalSlotForInternal (int internalIndex) const noexcept;

    int getChildProcessId() const noexcept;
    int getProcessId() const noexcept;

    void setStatusCallback (StatusCallback callback);
    void setSlotStateCallback (int slot, SlotStateCallback callback);
    void setRecoveredCallback (RecoveredCallback callback);
    void setActiveSlot (int slot) noexcept;
    int getActiveSlot() const noexcept;

    bool setStateInformation (int slot, const juce::MemoryBlock& state);
    bool setStateInformation (int slot, const void* data, int numBytes);
    juce::MemoryBlock getStateInformation (int slot);
    juce::MemoryBlock getLastCommittedState (int slot) const;
    std::uint64_t getLastCommittedStateRevision (int slot) const noexcept;
    bool flushPublishedState (int slot);
    void flushAllPublishedStates();

    bool setParameters (int slot, const std::vector<SandboxParameterEdit>& edits);
    bool openEditor (int slot, juce::String& error);
    bool closeEditor (int slot, juce::String& error);
    bool isEditorOpen (int slot) const noexcept;
    int getOpenEditorCount() const noexcept;

    int getSlotLatencySamples (int slot) const noexcept;
    int getSlotPluginLatencySamples (int slot) const noexcept;
    int getSlotUnderrunCount (int slot) const noexcept;
    int getSlotDroppedRequestCount (int slot) const noexcept;
    int getSlotDroppedMidiEventCount (int slot) const noexcept;
    std::int64_t getSlotPlayedBlockCount (int slot) const noexcept;
    SlotStatus getSlotStatus (int slot) const;

    bool requestRestart();
    bool restartAndWait (int timeoutMs, juce::String& error);

    void noteOn (int slot, int channel, int note, float velocity);
    void noteOff (int slot, int channel, int note);
    void allNotesOff (int slot);
    void prepareSlot (int slot, double sampleRate, int maximumBlockSize);
    void processSlot (int slot, int internalIndex, juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midi);

    sandbox::SharedMemory* getSlotChannel (int internalIndex) const noexcept;

    const Options& getOptions() const noexcept { return options; }
    juce::File getSharedMemoryFile() const noexcept { return sharedMemoryFile; }

    static const char* toString (Status status) noexcept;

private:
    friend class PageHostSlot;

    struct PendingMidiEvent
    {
        std::uint8_t bytes[3] { 0, 0, 0 };
        std::uint8_t numBytes = 0;
    };

    static constexpr int maxPendingMidiEvents = 256;

    struct SlotState;

    bool pushPendingEvent (SlotState& slot,
                           const std::uint8_t* bytes,
                           int numBytes) noexcept;

    struct SlotState
    {
        PageSlotDescriptor descriptor;
        int internalIndex = -1;

        mutable std::mutex stateLock;
        juce::MemoryBlock lastCommittedState;
        std::uint64_t lastCommittedRevision = 0;
        std::uint32_t seenEditorStateRevision = 0;
        juce::String localError;
        SlotStateCallback stateCallback;

        std::atomic<bool> failed { false };
        std::atomic<int> latencySamples { 0 };
        std::atomic<int> underruns { 0 };
        std::atomic<int> droppedRequests { 0 };
        std::atomic<int> droppedMidiEvents { 0 };
        std::atomic<std::int64_t> playedBlocks { 0 };
        std::atomic<int> controlInFlight { 0 };

        std::array<PendingMidiEvent, maxPendingMidiEvents> pendingMidi {};
        std::atomic<std::uint32_t> pendingWriteIndex { 0 };
        std::atomic<std::uint32_t> pendingReadIndex { 0 };

        std::uint64_t repliesPlayed = 0;
    };

    explicit PageHost (Options optionsToUse);

    bool startProcess (juce::String& error);
    void stopProcess (bool resetWinePrefix = false,
                      int helperStopTimeoutMs = 0) noexcept;
    void killChildProcessTree() noexcept;

    void startThreads();
    void stopThreads() noexcept;
    void watchdogLoop();
    void recoveryLoop();
    void runRecovery();
    bool recoverOnce (juce::String& error);

    void requestRecovery (const juce::String& reason, bool manual);
    void captureHealthyStates() noexcept;
    bool applyCommittedStates (juce::String& error);
    void markStatus (Status newStatus, const juce::String& reason);

    SlotState* findSlot (int slot) noexcept;
    const SlotState* findSlot (int slot) const noexcept;
    sandbox::SharedMemory* channelFor (SlotState& slot) const noexcept;

    bool abortRequested() const noexcept;
    bool runControlCommand (SlotState& slot,
                            sandbox::ControlCommand command,
                            const juce::MemoryBlock& input,
                            juce::MemoryBlock& output,
                            int timeoutMs);
    void commitState (SlotState& slot, const juce::MemoryBlock& state, bool notify);
    juce::File stateTransferFileFor (SlotState& slot) const;
    void closeRetiredMappings() noexcept;

    bool prepareBundleView (juce::String& error);
    void removeBundleView() noexcept;

    juce::File createPageFile() const;
    bool mapNewPage (juce::String& error);

    Options options;

    mutable std::mutex lifecycleMutex;
    mutable std::mutex statusLock;
    juce::String failureReason;
    StatusCallback statusCallback;
    RecoveredCallback recoveredCallback;

    std::atomic<Status> status { Status::stopped };
    std::atomic<int> activeSlot { -1 };
    std::atomic<int> preparedBlockSize { 0 };
    std::atomic<sandbox::SharedPage*> sharedPage { nullptr };
    std::atomic<int> childProcessId { 0 };

    juce::File sharedMemoryFile;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    std::vector<std::unique_ptr<juce::MemoryMappedFile>> retiredMappings;

    std::unique_ptr<juce::ChildProcess> childProcess;
    std::thread outputThread;
    std::atomic<bool> outputThreadStop { false };
    mutable std::mutex outputMutex;
    juce::String outputTail;

    std::vector<std::unique_ptr<SlotState>> slots;
    std::vector<int> internalToLogical;

    juce::File bundleViewRoot;
    juce::String vstbridgeGroup;
    juce::String winePrefix;
    std::map<juce::String, juce::File> bundleViews;

    std::thread watchdogThread;
    std::thread recoveryThread;
    mutable std::mutex recoveryMutex;
    std::condition_variable recoveryWake;
    bool shuttingDown = false;
    bool recoveryRequested = false;

    std::vector<juce::File> stateTransferFiles;
    std::vector<juce::File> editorStateFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PageHost)
};

} // namespace ihub::plugins
