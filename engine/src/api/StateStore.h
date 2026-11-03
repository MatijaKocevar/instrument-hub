#pragma once

#include "state/AudioState.h"
#include "state/FxState.h"
#include "state/LiveState.h"
#include "state/MidiState.h"
#include "state/PatchState.h"
#include "state/PluginState.h"
#include "state/TransportState.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ihub::settings
{
class SettingsStore;
}

namespace Api
{

class StateStore
{
public:
    struct Lifetime
    {
        std::mutex mutex;
        bool alive = true;
    };

    explicit StateStore (int numSlots = 64,
                         ihub::settings::SettingsStore* settingsStore = nullptr);
    ~StateStore();

    int getArmedSlot() const noexcept;

    std::uint64_t getRevision() const noexcept;

    std::shared_ptr<Lifetime> getLifetime() const noexcept;

    void notifyChanged (const char* kind, int slot = -1);

    bool setArmedSlot (int slot);

    using ArmHandler = std::function<void (int slot)>;
    void setArmHandler (ArmHandler handler);

    SettingsState getSettings() const;
    bool updateSettings (const juce::var& patch, std::string& error);

    ihub::settings::SettingsStore* getSettingsStore() const noexcept;
    bool persistAccountSettings (bool lanEnabled,
                                 const std::string& pairingToken);

    TransportState getTransport() const;
    bool updateTransport (const juce::var& patch, std::string& error);

    using TransportHandler = std::function<void (const TransportState& next,
                                                 const juce::var& patch)>;
    void setTransportHandler (TransportHandler handler);

    void setTransportProvider (TransportProvider provider);

    void setSlotRuntimeProvider (SlotRuntimeProvider provider);

    ProjectSaveState getProjectState() const;
    void setProjectStateProvider (ProjectStateProvider provider);
    void notifyProjectStateChanged();

    using ChangeHandler = std::function<void (const char* kind, int slot)>;
    void setChangeHandler (ChangeHandler handler);

    using EditHandler = std::function<void (const char* kind, int slot)>;
    void setEditHandler (EditHandler handler);

    using DetachHandler = std::function<void()>;
    void setDetachHandler (DetachHandler handler);

    int getNumSlots() const noexcept;
    void setSlotName (int slot, std::string name);
    void setSlotIdentity (int slot,
                          std::string family,
                          std::string familyName,
                          std::string patch,
                          std::string patchName);

    void setSlotLatency (int slot, int samples, double sampleRate);

    void setFamilyTree (std::vector<FamilyState> tree);
    std::vector<FamilyState> getFamilyTree() const;

    void setPatchInstrument (int slot, PatchInstrumentState instrument);

    void setPatchMix (int slot,
                      double volumeDb,
                      double pan,
                      bool mute,
                      bool solo,
                      int colorIndex);

    PatchState getPatch (int slot) const;

    bool updatePatch (int slot, const juce::var& patch, std::string& error);

    using PatchSourceHandler = std::function<bool (int slot,
                                                   const PatchSourceRequest& source,
                                                   std::string& error)>;
    void setPatchSourceHandler (PatchSourceHandler handler);

    bool applyPatchSource (int slot,
                           const PatchSourceRequest& source,
                           std::string& error);

    using PatchEditorHandler = std::function<bool (int slot, std::string& error)>;
    void setPatchEditorHandlers (PatchEditorHandler openEditor,
                                 PatchEditorHandler closeEditor);

    bool openPatchEditor (int slot, std::string& error);

    bool closePatchEditor (int slot, std::string& error);

    using PatchStateCaptureHandler =
        std::function<bool (int slot, PatchStateCapture& capture, std::string& error)>;
    void setPatchStateCaptureHandler (PatchStateCaptureHandler handler);

    bool capturePatchState (int slot,
                            PatchStateCapture& capture,
                            std::string& error);

    using PatchHandler = std::function<void (int slot,
                                             const PatchState& patch,
                                             float gain)>;
    void setPatchHandler (PatchHandler handler);

    void setFxHandlers (FxChainProvider provider, FxCommandHandler handler);

    bool getFxChain (int slot, FxChainState& chain, std::string& error) const;

    bool applyFxCommand (int slot,
                         const FxCommand& command,
                         FxChainState& chain,
                         std::string& error);

    StateSnapshot snapshot() const;
    double getUptimeMs() const noexcept;

private:
    struct PatchMixState
    {
        double volumeDb = 0.0;
        double pan = 0.0;
        bool mute = false;
        bool solo = false;
        int colorIndex = 0;
    };

    PatchState makePatchLocked (int slot) const;
    void collectEffectiveGainsLocked (const std::vector<PatchMixState>& mixes,
                                      std::vector<float>& gains) const;
    void notifyEdited (const char* kind, int slot);

    void loadStoredSettings();
    bool persistMixerSettings (const SettingsState& next, std::string& error);

    mutable std::mutex mutex;

    const int numSlots;
    ihub::settings::SettingsStore* const settingsStore;
    int armedSlot = -1;
    std::vector<std::string> slotNames;
    std::vector<std::string> slotFamilies;
    std::vector<std::string> slotFamilyNames;
    std::vector<std::string> slotPatches;
    std::vector<std::string> slotPatchNames;
    std::vector<int> slotLatencySamples;
    std::vector<double> slotLatencyMs;
    std::vector<PatchInstrumentState> slotInstruments;
    std::vector<PatchMixState> slotMixes;
    std::vector<bool> slotEditorOpen;
    std::vector<FamilyState> families;
    SettingsState settings;
    TransportState transport;

    ArmHandler armHandler;
    TransportHandler transportHandler;
    ChangeHandler changeHandler;
    EditHandler editHandler;
    DetachHandler detachHandler;
    PatchHandler patchHandler;
    PatchSourceHandler patchSourceHandler;
    PatchEditorHandler patchEditorHandler;
    PatchEditorHandler patchEditorCloseHandler;
    PatchStateCaptureHandler patchStateCaptureHandler;
    FxChainProvider fxProvider;
    FxCommandHandler fxHandler;
    TransportProvider transportProvider;
    SlotRuntimeProvider slotRuntimeProvider;
    ProjectStateProvider projectStateProvider;

    std::atomic<std::uint64_t> revision { 0 };
    std::shared_ptr<Lifetime> lifetime = std::make_shared<Lifetime>();

    const std::chrono::steady_clock::time_point startedAt;
};
}
