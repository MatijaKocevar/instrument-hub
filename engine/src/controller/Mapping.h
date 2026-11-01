#pragma once

#include "MappingModel.h"
#include "MappingYaml.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace Controller
{

//==============================================================================
// Live mapping
//==============================================================================

/** Resolves controller events to slots and macros, and hot-reloads `set.yaml`.

    ## Routing

    A pad press arms the slot of that pad's instrument. The armed slot stays
    armed after the pad is released (it is a *selection*, not a moment). Keys
    and incoming MIDI follow the armed slot through `SlotSwitcher`.

    Encoders are resolved against the active pad's slot: encoder 0 with macro
    `cutoff` becomes a `MacroChange` for the slot the last pad selected.

    ## Control rows

    The Launchpad has no encoders and no row of transport keys. Its top row
    (`CC 91..98`) selects one of the eight macro pages declared under `banks`,
    and its right column (`CC 89..19`, top to bottom) selects one of the eight
    scenes declared under `scenes`. Both are controller events: they change
    what the UI exposes, never the audio graph.

    ## Hot reload

    `startWatching()` runs one private thread. It polls the file every
    `intervalMs` (25 ms by default), parses a complete replacement, and
    publishes it atomically. Nothing ever touches the audio stream's lifetime:
    the only thing that crosses to audio is the lock-free
    `SlotSwitcher::armSlot()` call made by the `armSlot` listener. A malformed
    file keeps the previous mapping and reports an error.

    If the active pad's binding changed, the new slot is armed as part of the
    reload. That is what makes "edit the instrument, hear the new one" work
    without pressing the pad again.

    ## Threading

    - `load()`, `reload()`, `poll()`, `startWatching()`, `stopWatching()`,
      `setListeners()` are control-thread calls.
    - `handlePad()` and `handleEncoder()` may be called from the MIDI input
      thread; they take a short mutex, never block on I/O, and invoke listeners
      after releasing it.
    - Listeners run on the calling thread (the watcher thread for a reload).
      `armSlot` is a control-thread callback: it should only call lock-free
      control methods such as `SlotSwitcher::armSlot()`, never the audio graph.
    - `getMapping()` returns an immutable snapshot; holding it keeps the parse
      alive across a reload.
*/
class Mapping final
{
public:
    /** What one encoder movement means. */
    struct MacroChange
    {
        int slot = -1;           // active slot, or -1 when no pad is selected
        MacroSpec macro;
        int rawValue = 0;        // 0..127 as received from the controller
        float normalized = 0.0f; // 0..1
        float value = 0.0f;      // scaled into [macro.minimum, macro.maximum]
    };

    struct Listeners
    {
        /** Arms `slot`; `-1` disarms. Called on pad press and on a reload that
            changes the active pad's binding. */
        std::function<void (int slot)> armSlot;

        /** A pad went down or up. `binding` is null for an unmapped pad and
            only valid for the duration of the call. */
        std::function<void (int pad, const PadBinding* binding, bool pressed)> padEvent;

        /** An encoder moved and resolved to a macro. */
        std::function<void (const MacroChange& change)> macroEvent;

        /** A top-row button selected `bank`. `page` is valid for the call. */
        std::function<void (int bank, const MacroBankSpec* page)> bankEvent;

        /** A right-column button selected `scene`. `selection` is valid for
            the call. */
        std::function<void (int scene, const SceneSpec* selection)> sceneEvent;

        /** A new document was published. `hotReload` is false for the first
            successful load. */
        std::function<void (const std::shared_ptr<const SetMapping>& mapping,
                            bool hotReload)> changed;

        /** The file could not be read or parsed. The previous mapping stays
            active. Repeated identical failures are reported once. */
        std::function<void (const MappingError& error)> error;
    };

    static constexpr int defaultPollIntervalMs = 25;

    Mapping();
    ~Mapping();

    Mapping (const Mapping&) = delete;
    Mapping& operator= (const Mapping&) = delete;

    //==============================================================================
    // Lifecycle

    void setListeners (Listeners newListeners);

    /** Loads `file` and publishes it. Does not start watching. */
    bool load (const std::filesystem::path& file, MappingError* error = nullptr);

    /** Re-reads the current file and applies it even if its content is
        unchanged. */
    bool reload (MappingError* error = nullptr);

    /** Reads the file once and applies it if the content changed.

        Returns true when a new document was published. Safe to call from a
        timer instead of `startWatching()` when the host owns the message loop.
    */
    bool poll();

    /** Starts the polling watcher. Returns false if the interval is invalid or
        the thread cannot be started. Calling it twice is a no-op. */
    bool startWatching (int intervalMs = defaultPollIntervalMs);

    void stopWatching();
    bool isWatching() const noexcept;

    //==============================================================================
    // Inspection

    std::filesystem::path getPath() const;
    std::shared_ptr<const SetMapping> getMapping() const;

    /** Incremented for every successfully published document. */
    std::uint64_t getRevision() const noexcept;

    MappingError getLastError() const;

    std::optional<PadBinding> getPadBinding (int pad) const;
    std::optional<EncoderBinding> getEncoderBinding (int encoder) const;
    std::optional<MacroBankSpec> getBank (int bank) const;
    std::optional<SceneSpec> getScene (int scene) const;

    /** The pad that selected the armed slot, or -1 before the first press. */
    int getActivePad() const noexcept;

    /** The slot the active pad currently resolves to, or -1. */
    int getArmedSlot() const noexcept;

    /** The selected top-row macro page, or -1 before the first selection. */
    int getActiveBank() const noexcept;

    /** The selected right-column scene, or -1 before the first selection. */
    int getActiveScene() const noexcept;

    //==============================================================================
    // Controller events

    /** Handles a pad down/up. Returns true when the pad is mapped and a press
        armed a slot. */
    bool handlePad (int pad, bool pressed);

    /** Handles a raw 0..127 encoder value. Returns true when it resolved to a
        macro. */
    bool handleEncoder (int encoder, int value);

    /** Handles a top-row / right-column / logo CC. Returns true when a press
        selected a macro page or scene. */
    bool handleButton (int cc, bool pressed);

    /** Handles a top-row button by index 0..7 (left to right). */
    bool handleTopRow (int index, bool pressed);

    /** Handles a right-column button by index 0..7 (top to bottom). */
    bool handleRightColumn (int index, bool pressed);

    //==============================================================================
    // Default set file discovery

    /** `$INSTRUMENT_HUB_SET`, else `<cwd>/set.yaml`, else
        `<cwd>/shared/set.yaml`. Empty when none of those exist. */
    static std::filesystem::path findDefaultSetFile();

private:
    bool applyText (const std::string& text,
                    bool hotReload,
                    std::uint64_t contentHash,
                    bool force,
                    MappingError* error);
    bool reloadInternal (bool hotReload, MappingError* error);
    void reportError (const MappingError& error, std::uint64_t contentHash,
                      bool rememberHash);
    void watchLoop (int intervalMs);

    mutable std::mutex mutex;
    Listeners listeners;

    std::filesystem::path path;
    std::shared_ptr<const SetMapping> current;

    MappingError lastError;
    std::atomic<std::uint64_t> revision { 0 };
    std::uint64_t lastHash = 0;
    bool hasAttempted = false;
    bool hasLoaded = false;
    std::atomic<int> activePad { -1 };
    std::atomic<int> activeBank { -1 };
    std::atomic<int> activeScene { -1 };

    std::mutex watchMutex;
    std::thread watcher;
    std::atomic<bool> quitWatcher { false };
    std::atomic<bool> watching { false };
};

} // namespace Controller
