#pragma once

#include "state/AudioState.h"
#include "state/FxState.h"
#include "state/LiveState.h"
#include "state/MidiState.h"
#include "state/PatchState.h"
#include "state/PluginState.h"
#include "state/TransportState.h"

#include <juce_core/juce_core.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Api
{

/**
    Thread-safe control state behind the HTTP API.

    Every method is safe to call from the HTTP server threads, the UI and the
    message thread. The store never touches the audio thread; `setArmHandler()`
    and `setTransportHandler()` bridge requests into the lock-free graph
    controls, and the runtime providers let `snapshot()` read the real session
    state without putting a lock on the audio path.
*/
class StateStore
{
public:
    explicit StateStore (int numSlots = 64);

    //==============================================================================
    // Arm

    int getArmedSlot() const noexcept;

    /** Arms `slot`, or disarms when `slot` is negative.

        Returns false (and changes nothing) when `slot` is not a valid index and
        not negative.
    */
    bool setArmedSlot (int slot);

    /** Optional listener called after a successful `setArmedSlot()`.

        It runs outside the store's lock. The handler must be lock-free if it
        crosses into the audio graph; `SlotSwitcher::armSlot()` is the intended
        target.
    */
    using ArmHandler = std::function<void (int slot)>;
    void setArmHandler (ArmHandler handler);

    //==============================================================================
    // Settings / transport

    SettingsState getSettings() const;
    bool updateSettings (const juce::var& patch, std::string& error);

    TransportState getTransport() const;
    bool updateTransport (const juce::var& patch, std::string& error);

    /** Applies the accepted patch to the audio graph.

        `next` is the merged store state and `patch` is the original JSON
        object, so the handler can apply only the fields the request changed
        and leave a running position alone when the request only toggles
        `playing`.
    */
    using TransportHandler = std::function<void (const TransportState& next,
                                                 const juce::var& patch)>;
    void setTransportHandler (TransportHandler handler);

    /** Reads `GET /transport` and `/state` from the session instead of the
        synthetic store values. Must be lock-free if the session is running.
    */
    void setTransportProvider (TransportProvider provider);

    /** Reads each slot's armed flag, crossfade gain and graph state from the
        session. Replaces the synthetic `armed` value in `snapshot()`.
    */
    void setSlotRuntimeProvider (SlotRuntimeProvider provider);

    //==============================================================================
    // Change notification (T103)

    /** Called after every successful mutation of the store.

        `kind` is `"arm"`, `"settings"`, `"transport"`, `"slot"` or `"patch"`;
        `slot` is the affected slot for `"arm"`/`"slot"`/`"patch"` and -1
        otherwise. The handler runs outside the store's lock, on the thread that
        made the change, so it must not block the audio path (it never is on
        it).

        `StateBroadcast` installs this to mirror API and controller changes to
        the connected clients. There is one handler per store.
    */
    using ChangeHandler = std::function<void (const char* kind, int slot)>;
    void setChangeHandler (ChangeHandler handler);

    //==============================================================================
    // Slots

    int getNumSlots() const noexcept;
    void setSlotName (int slot, std::string name);
    void setSlotIdentity (int slot,
                          std::string family,
                          std::string familyName,
                          std::string patch,
                          std::string patchName);

    /** Publishes the latency of one slot's instrument + FX chain.

        Pass the slot's current sample rate; it is used for the millisecond
        readout. Values are clamped to zero and ignored for an invalid slot.
        Call from the control/message thread after a chain change, never from
        the audio callback.
    */
    void setSlotLatency (int slot, int samples, double sampleRate);

    /** Publishes the family -> patch tree that `/state` mirrors. */
    void setFamilyTree (std::vector<FamilyState> tree);
    std::vector<FamilyState> getFamilyTree() const;

    //==============================================================================
    // Patch mix / colour (T120)

    /** Seeds the instrument descriptor of one patch. Called when a set is
        composed; it does not broadcast a change. Ignored for an invalid slot.
    */
    void setPatchInstrument (int slot, PatchInstrumentState instrument);

    /** Seeds the persistent mixer/colour state of one patch.

        Values are clamped to the API ranges (`volumeDb` -60..12, `pan` -1..1,
        `colorIndex` 0..15). Called when a set is composed; it does not
        broadcast a change.
    */
    void setPatchMix (int slot,
                      double volumeDb,
                      double pan,
                      bool mute,
                      bool solo,
                      int colorIndex);

    /** The stored patch, without live warm/latency values. */
    PatchState getPatch (int slot) const;

    /** Applies a partial patch update from `PATCH /patches/{slot}`.

        Accepts `volumeDb`, `pan`, `mute` (or `muted`), `solo` and `colorIndex`.
        Unknown fields and out-of-range values are rejected before anything is
        applied. On success every slot's effective gain is recomputed (mute and
        solo are global) and the patch handler is called once per slot.
    */
    bool updatePatch (int slot, const juce::var& patch, std::string& error);

    /** Publishes the source handler used by `POST /patches/{slot}/source`.

        The handler swaps the live source in the fixed graph. It runs outside
        the store's lock, on the HTTP thread, and must not run on the audio
        thread. Empty means no audio session is attached.
    */
    using PatchSourceHandler = std::function<bool (int slot,
                                                   const PatchSourceRequest& source,
                                                   std::string& error)>;
    void setPatchSourceHandler (PatchSourceHandler handler);

    /** Applies one instrument-source change and its descriptor update.

        Calls the source handler first; nothing is changed when it refuses the
        request. On success the patch descriptor is replaced, the change is
        broadcast as `event.kind: "patch"`, and the source is returned in the
        next `snapshot()`.
    */
    bool applyPatchSource (int slot,
                           const PatchSourceRequest& source,
                           std::string& error);

    //==========================================================================
    // Native plugin editor (T129)

    /** Opens or closes the patch's native editor window.

        The handler resolves the live hosted plugin, marshals to the message
        thread and returns a readable error when the window cannot be created.
        It runs on an HTTP thread, never the audio thread. Empty means no audio
        session is attached.
    */
    using PatchEditorHandler = std::function<bool (int slot, std::string& error)>;
    void setPatchEditorHandlers (PatchEditorHandler openEditor,
                                 PatchEditorHandler closeEditor);

    /** Applies `POST /patches/{slot}/editor`. */
    bool openPatchEditor (int slot, std::string& error);

    /** Applies `DELETE /patches/{slot}/editor`. Idempotent. */
    bool closePatchEditor (int slot, std::string& error);

    //==========================================================================
    // Explicit patch-state capture (T132)

    /** Saves the running plugin's current sound into its patch.

        The handler captures the resident instance's state and writes the
        patch's sidecar blob immediately. It runs on an HTTP thread, never the
        audio thread. Empty means no audio session is attached.
    */
    using PatchStateCaptureHandler =
        std::function<bool (int slot, PatchStateCapture& capture, std::string& error)>;
    void setPatchStateCaptureHandler (PatchStateCaptureHandler handler);

    /** Applies `POST /patches/{slot}/state/capture`. */
    bool capturePatchState (int slot,
                            PatchStateCapture& capture,
                            std::string& error);

    /** Applies a patch's effective linear gain to the audio graph.

        `gain` already includes the mute/solo decision, so the handler only has
        to publish it to the slot. It runs outside the store's lock, on the
        thread that made the change, and must be lock-free (never the audio
        thread).
    */
    using PatchHandler = std::function<void (int slot,
                                             const PatchState& patch,
                                             float gain)>;
    void setPatchHandler (PatchHandler handler);

    //==============================================================================
    // Per-patch insert chain (T121)

    /** Installs the live chain reader and editor. Called by `attachSession()`.

        Both run outside the store's lock. The handler must be safe to call
        from an HTTP thread and must not be called from the audio thread.
    */
    void setFxHandlers (FxChainProvider provider, FxCommandHandler handler);

    /** Reads one patch's chain. Returns false with a message for a bad slot or
        when no audio session is attached. */
    bool getFxChain (int slot, FxChainState& chain, std::string& error) const;

    /** Applies one chain mutation and returns the resulting chain. */
    bool applyFxCommand (int slot,
                         const FxCommand& command,
                         FxChainState& chain,
                         std::string& error);

    //==============================================================================
    // Inspection

    StateSnapshot snapshot() const;
    double getUptimeMs() const noexcept;

private:
    /** Persistent mixer/colour state of one slot (T120). */
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

    mutable std::mutex mutex;

    const int numSlots;
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
    PatchHandler patchHandler;
    PatchSourceHandler patchSourceHandler;
    PatchEditorHandler patchEditorHandler;
    PatchEditorHandler patchEditorCloseHandler;
    PatchStateCaptureHandler patchStateCaptureHandler;
    FxChainProvider fxProvider;
    FxCommandHandler fxHandler;
    TransportProvider transportProvider;
    SlotRuntimeProvider slotRuntimeProvider;

    const std::chrono::steady_clock::time_point startedAt;
};
} // namespace Api
