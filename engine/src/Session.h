#pragma once

#include "SessionConfig.h"
#include "SessionSwitchReport.h"
#include "api/state/AudioState.h"
#include "api/state/PluginPageState.h"
#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "audio/FxRack.h"
#include "audio/SlotSwitcher.h"
#include "instruments/Instrument.h"
#include "session/DeviceController.h"
#include "session/FxChainController.h"
#include "session/MeterBridge.h"
#include "session/MidiBridge.h"
#include "session/PluginStateStore.h"
#include "session/TransportControl.h"
#include "session/WarmPoolController.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace juce {
class AudioPluginFormatManager;
}

namespace ihub::plugins {
class PageHost;
class PageHostSlot;
class PluginInstrument;
class SandboxHost;
class SandboxSlotControl;
}  // namespace ihub::plugins

namespace ihub::session {
class EditorHost;
class MirrorSync;
}  // namespace ihub::session

namespace ihub {

class Session final : public juce::AudioIODeviceCallback {
public:
    static constexpr int defaultLogicalSlots = 64;
    static constexpr int defaultTrackSlots = 8;

    Session();
    ~Session() override;

    static std::vector<SessionSlotConfig> makeDefaultSet(
        SessionSlotKind defaultKind = SessionSlotKind::empty);

    bool build(const SessionConfig& config, juce::String& error, bool keepDeviceOpen = false);

    void prepare(double sampleRate, int maximumBlockSize, int numChannels);

    void resetGraph() noexcept;

    bool openDevice(juce::String* errorMessage = nullptr);

    bool openDevice(const SessionDeviceConfig& device, juce::String* errorMessage = nullptr);

    void closeDevice();

    void beginShutdown();

    bool isDeviceOpen() const noexcept { return devices.isOpen(); }

    AudioEngine::DeviceManager& getDevices() noexcept { return devices.getManager(); }

    bool isPrepared() const noexcept { return prepared; }
    int getNumSlots() const noexcept;

    int getLogicalSlotCount() const noexcept { return static_cast<int>(config.slots.size()); }

    AudioEngine::AudioEngine& getEngine() noexcept;
    const AudioEngine::AudioEngine& getEngine() const noexcept;

    AudioEngine::SlotSwitcher& getSwitcher() noexcept;
    const AudioEngine::SlotSwitcher& getSwitcher() const noexcept;

    AudioEngine::Transport& getTransport() noexcept { return transportControl.getTransport(); }
    const AudioEngine::Transport& getTransport() const noexcept {
        return transportControl.getTransport();
    }

    float getSlotMeterPeak(int slot) const noexcept;

    float getMasterMeterPeak() const noexcept;

    const SessionConfig& getConfig() const noexcept { return config; }
    SessionLayout getLayout() const noexcept { return config.layout; }
    const std::vector<SessionSlotConfig>& getSlotConfigs() const noexcept { return config.slots; }

    std::vector<SessionSlotConfig> copySourceConfigs() const;

    int getSlotLatencySamples(int slot) const noexcept;

    bool setSlotSource(int slot, const SessionSlotConfig& source, std::string& error);

    using SourceAssignedHook = std::function<void(int slot, const SessionSlotConfig& source)>;
    void setSourceAssignedHook(SourceAssignedHook hook);

    using EditorClosedHook = std::function<void(int slot)>;
    void setEditorClosedHook(EditorClosedHook hook);

    bool openEditor(int slot, std::string& error);

    bool closeEditor(int slot, std::string& error);

    bool isEditorOpen(int slot) const noexcept;

    bool reloadPlugin(int slot, std::string& error);

    int getOpenEditorCount() const noexcept;

    void setPluginStateDirectory(const juce::File& directory);

    juce::File getPluginStateDirectory() const { return pluginState.getDirectory(); }

    bool loadPluginStates();

    bool savePluginStates();

    bool capturePluginState(int slot);

    bool copyPluginStateData(int slot, juce::MemoryBlock& data) const;

    bool hasPluginState(int slot) const noexcept;

    juce::int64 getPluginStateSize(int slot) const noexcept;

    plugins::PluginInstrument* getPluginInstrument(int slot);

    plugins::SandboxHost* getSandboxHost(int slot) const noexcept;

    plugins::SandboxSlotControl* getSandboxSlotControl(int slot) const noexcept;

    std::shared_ptr<plugins::PageHost> getPageHost() const;

    bool isSharedPluginHostEnabled() const noexcept { return sharedPluginHostEnabled; }

    std::vector<Api::PluginPageStatus> getPluginPageStatuses() const;

    bool restartPluginPage(const std::string& pageId, std::string& error);

    int getSandboxProcessId(int slot) const noexcept;

    juce::String getSlotError(int slot) const;

    bool isSlotPluginFailed(int slot) const noexcept;

    bool getSlotFxChain(int slot, AudioEngine::FxChainSnapshot& snapshot, std::string& error) const;

    bool getSlotTunerReading(int slot, int index, AudioEngine::FxTunerReading& reading,
                             std::string& error) const;

    bool addSlotFx(int slot, const AudioEngine::FxInsertConfig& insert, int position,
                   AudioEngine::FxChainSnapshot& snapshot, std::string& error);

    bool replaceSlotFx(int slot, const std::vector<AudioEngine::FxInsertConfig>& chain,
                       AudioEngine::FxChainSnapshot& snapshot, std::string& error);

    bool updateSlotFx(int slot, int index, const AudioEngine::FxInsertPatch& patch,
                      AudioEngine::FxChainSnapshot& snapshot, std::string& error);

    bool removeSlotFx(int slot, int index, AudioEngine::FxChainSnapshot& snapshot,
                      std::string& error);

    bool reorderSlotFx(int slot, const std::vector<int>& order,
                       AudioEngine::FxChainSnapshot& snapshot, std::string& error);

    bool clearSlotFx(int slot, AudioEngine::FxChainSnapshot& snapshot, std::string& error);

    void updateFxTransport() noexcept;

    int getNeighbourCount() const noexcept;
    int getWarmInstanceLimit() const noexcept;
    int getLiveInstrumentCount() const noexcept;

    int getWarmProtectedCount() const noexcept;
    std::vector<int> getWarmProtectedSlots() const;
    std::uint64_t getWarmCreationCount() const noexcept;
    std::uint64_t getWarmEvictionCount() const noexcept;

    AudioEngine::WarmPool::PreloadReport preloadAll();
    Api::AudioMeasurementState measureLatency();
    bool waitForWarmIdle(int timeoutMs = 10000);

    int getPeakInstrumentCount() const noexcept;

    bool isSlotWarm(int slot) const noexcept;

    SessionWarmState getWarmState(int slot) const noexcept;

    const char* getWarmStateName(int slot) const noexcept;

    using SlotStateObserver = session::WarmPoolController::SlotStateObserver;
    void setSlotStateObserver(SlotStateObserver observer);

    using PageStatusObserver = std::function<void()>;
    void setPageStatusObserver(PageStatusObserver observer);

    bool waitForSlotWarm(int slot, int timeoutMs = 5000);

    bool waitForSlotActive(int slot, int timeoutMs = 5000);

    void armSlot(int slot);

    int getArmedSlot() const noexcept;

    void setSlotGain(int slot, float linearGain) noexcept;

    bool pushMidi(const juce::MidiMessage& message) noexcept;

    int getQueuedMidiCount() const noexcept { return midi.getQueuedCount(); }

    void render(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi, int numSamples) noexcept;

    SessionSwitchReport measureSwitch(int fromSlot, int toSlot, int settleBlocks = 8,
                                      int rmsBlocks = 8, int switchBlocks = 32);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
        int numOutputChannels, int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

private:
    std::unique_ptr<AudioEngine::SlotSource> createPooledSource(int slotIndex, juce::String& error);

    std::unique_ptr<AudioEngine::SlotSource> createSharedPageSlot(int slotIndex,
                                                                  juce::String& error);

    std::shared_ptr<plugins::PageHost> ensurePageHost(juce::String& error);
    void resetPageHost() noexcept;
    void configurePageCallbacks(plugins::PageHost& host);
    void onPageStatusChanged(int status, const juce::String& reason);
    void onPageSlotStateChanged(int slot, const juce::MemoryBlock& state,
                                const session::PluginStateIdentity& identity);
    void onPageRecovered(int activeSlot);
    void retryPageWarmSlots();

    void renderInternal(int numSamples, juce::MidiBuffer& midi) noexcept;
    void copyToOutput(float* const* outputChannelData, int numOutputChannels,
                      int numSamples) noexcept;

    void capturePluginStateForInstance(AudioEngine::SlotSource& instance);
    bool pluginIdentityForSlot(int slot, session::PluginStateIdentity& identity) const;

    bool ensurePluginMirror(int slot, std::string& error);
    bool ensurePluginMirrorNow(int slot, std::string& error);
    void releasePluginMirror(int slot);
    void releaseAllPluginMirrors();
    void syncMirrorParameter(int slot, const juce::String& id, float normalisedValue);
    void syncMirrorState(int slot);
    juce::MemoryBlock copyMirrorState(int slot) const;
    void cancelPendingLoads();

    void registerSandboxSlot(int slot, plugins::SandboxHost& host,
                             const session::PluginStateIdentity& identity);
    void unregisterSandboxSlot(plugins::SandboxHost& host);
    plugins::SandboxHost* sandboxForSlot(int slot) const noexcept;
    void closeSandboxEditors(int keepSlot);
    bool waitForPageSlot(int slot, int timeoutMs);

    std::uint64_t nextSandboxGeneration(int slot);
    std::uint64_t bumpSandboxGeneration(int slot);
    bool isCurrentSandboxGeneration(int slot, std::uint64_t generation) const noexcept;

    void notifySlotStateChanged(int slot);

    void setSlotError(int slot, const juce::String& error);
    void clearSlotError(int slot);

    bool isStreamActive() const noexcept { return devices.isOpen() && devices.hasCallback(); }

    SessionConfig config;

    std::vector<SessionSlotConfig> sourceConfigs;
    mutable std::mutex sourceMutex;
    std::uint64_t sourceRevision = 0;

    SourceAssignedHook sourceAssignedHook;
    mutable std::mutex sourceAssignedMutex;

    EditorClosedHook editorClosedHook;
    mutable std::mutex editorClosedMutex;

    std::unique_ptr<session::EditorHost> editorHost;
    std::unique_ptr<session::MirrorSync> mirrorSync;

    session::PluginStateStore pluginState{defaultLogicalSlots};

    struct SandboxSlot {
        int slot = -1;
        plugins::SandboxHost* host = nullptr;
        session::PluginStateIdentity identity;
        std::uint64_t generation = 0;
    };

    mutable std::mutex sandboxMutex;
    std::vector<SandboxSlot> sandboxSlots;
    std::vector<std::uint64_t> sandboxGenerations;
    std::uint64_t sandboxGenerationCounter = 0;

    std::vector<std::uint64_t> pluginReloadRevisions;

    mutable std::mutex slotObserverMutex;
    session::WarmPoolController::SlotStateObserver slotStateObserver;

    mutable std::mutex pageStatusObserverMutex;
    PageStatusObserver pageStatusObserver;

    mutable std::mutex pageHostMutex;
    std::mutex pageStartMutex;
    std::shared_ptr<plugins::PageHost> pageHost;

    mutable std::mutex pageSlotHandleMutex;
    std::vector<plugins::PageHostSlot*> pageSlotHandles;

    bool sharedPluginHostEnabled = false;

    mutable std::mutex mirrorMutex;
    std::vector<std::shared_ptr<plugins::PluginInstrument>> pluginMirrors;

    mutable std::mutex slotErrorMutex;
    std::vector<juce::String> slotErrors;

    std::unique_ptr<juce::AudioPluginFormatManager> pluginFormats;
    session::WarmPoolController warmPool;
    session::FxChainController fxChain;

    std::unique_ptr<AudioEngine::AudioEngine> engine;
    std::unique_ptr<AudioEngine::SlotSwitcher> switcher;
    session::DeviceController devices;
    session::TransportControl transportControl;
    session::MeterBridge meters;
    session::MidiBridge midi;

    juce::AudioBuffer<float> renderBuffer;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 2;
    bool prepared = false;
    std::atomic<bool> shuttingDown{false};
    std::shared_ptr<std::atomic<bool>> loadAbort;

    static constexpr int latencyProbeCaptureSamples = 16384;
    std::vector<float> latencyProbeInput;
    std::atomic<bool> latencyProbeRequested{false};
    std::atomic<bool> latencyProbeActive{false};
    std::atomic<bool> latencyProbeComplete{false};
    std::atomic<bool> latencyProbeEmit{false};
    std::atomic<bool> latencyProbeBusy{false};
    std::atomic<int> latencyProbePosition{0};
    std::atomic<int> latencyProbeInputChannels{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Session)
};

}  // namespace ihub
