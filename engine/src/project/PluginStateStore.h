#pragma once

#include "SetFile.h"

#include "../audio/AudioEngine.h"
#include "../plugins/PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ihub::project
{

/** Why a slot could not be bound to a live plugin instance. */
enum class PluginLoadWarningKind
{
    /** The set names a plugin that the host does not know about. */
    missingPlugin = 0,

    /** The slot references a state id that is not in the set. */
    missingState,

    /** The plugin is known but creating an instance failed. */
    loadFailed,

    /** The instance loaded, but rejected the stored state blob. */
    stateRejected
};

const char* toString (PluginLoadWarningKind kind) noexcept;

/** One non-fatal problem found while binding a set to the engine.

    A warning never stops the load: the slot stays in the graph with a silent
    placeholder, so the rest of the session still plays and the UI can show
    what is missing.
*/
struct PluginLoadWarning
{
    PluginLoadWarningKind kind = PluginLoadWarningKind::missingPlugin;
    int slotIndex = -1;
    std::string pluginStateId;
    std::string pluginId;
    std::string pluginName;
    juce::String message;

    bool operator== (const PluginLoadWarning&) const = default;
};

/** The result of binding one slot.

    `placeholder` is true when the slot is present in the graph but silent
    because its plugin or its state could not be restored.
*/
struct SlotBinding
{
    int slotIndex = -1;
    std::string pluginStateId;
    std::string pluginId;
    std::string pluginName;

    /** Version stamp copied from the plugin state chunk. */
    std::string pluginVersion;

    bool placeholder = false;
    juce::String warning;

    bool operator== (const SlotBinding&) const = default;
};

/** Outcome of `PluginStateStore::loadInto()`.

    Missing plugins are reported in `warnings` and do not fail the load; only
    a set that cannot be applied to the engine at all sets `success` to false.
*/
struct PluginLoadReport
{
    bool success = true;
    juce::String error;

    std::vector<SlotBinding> bindings;
    std::vector<PluginLoadWarning> warnings;

    int getNumLoaded() const noexcept;
    int getNumPlaceholders() const noexcept;
    bool hasWarnings() const noexcept { return ! warnings.empty(); }

    /** Human-readable summary, one line per binding and warning. */
    juce::String toString() const;
};

/**
    Owns the live plugin instances for a loaded set and their state blobs.

    A `SetFile` stores opaque plugin state keyed by a UUID. This class is the
    bridge between that document and the audio graph:

    - **Load.** `loadInto()` applies the set's slot settings, resolves each
      plugin instrument against `availablePlugins` (or a custom resolver),
      creates a `PluginInstrument`, restores the state blob, prepares it and
      installs it as the slot's `SlotSource`.
    - **Capture.** `captureState()` takes a live instance and produces a new
      `PluginStateChunk`: a fresh UUID, the plugin identity (format, id, name,
      manufacturer) and the plugin's own version stamp, plus the opaque bytes
      from `PluginInstrument::getStateInformation()`.
    - **Missing plugins.** When the plugin is not installed, or its state
      cannot be restored, the slot gets a silent placeholder and a warning in
      the report. Nothing is dereferenced, nothing throws, and no other slot
      is affected. This is the recovery path for a set opened on a machine
      without the plugins that made it.
    - **Lifetime.** `unload()` detaches every source from the engine before
      destroying the instances. The audio stream must be stopped while
      `loadInto()` and `unload()` run. If the store is destroyed without an
      explicit `unload()`, the caller must destroy the engine (or at least
      stop it and clear its sources) first: a `Slot` only borrows the source.

    All methods are message-thread calls; nothing here touches the audio
    thread's contract.
*/
class PluginStateStore final
{
public:
    /** Version of the store's binding/capture behaviour.

        The per-chunk plugin version travels in `PluginStateChunk::version`;
        this constant is the host-side format stamp used when a later
        migration needs to distinguish state written by different store
        generations.
    */
    static constexpr int formatVersion = 1;

    /** Resolves a stored chunk to a plugin description.

        Returning false means "not installed"; the load installs a placeholder
        and records a `missingPlugin` warning.
    */
    using PluginResolver = std::function<bool (const PluginStateChunk&, juce::PluginDescription&)>;

    PluginStateStore();
    ~PluginStateStore();

    //==============================================================================
    // Configuration (message thread, before loadInto())

    /** Types the host knows about. The default resolver matches these by
        format and plugin id (then by name, manufacturer and version). */
    void setAvailablePlugins (std::vector<juce::PluginDescription> descriptions);
    const std::vector<juce::PluginDescription>& getAvailablePlugins() const noexcept
    {
        return availablePlugins;
    }

    /** Overrides the default lookup. Used by tests and by hosts with their own
        plugin database. */
    void setResolver (PluginResolver resolver);
    void clearResolver();

    //==============================================================================
    // Load / unload

    /** Applies `set` to `engine` and binds every plugin instrument slot.

        The engine must own the same number of slots as the set. `sampleRate`
        and `maximumBlockSize` are only used when the engine has not been
        prepared yet; otherwise its prepared values win. The stream must be
        stopped. Returns a report even when some plugins are missing.
    */
    PluginLoadReport loadInto (const SetFile& set,
                               AudioEngine::AudioEngine& engine,
                               double sampleRate = 0.0,
                               int maximumBlockSize = 0);

    /** Clears every slot source owned by the store, then destroys the
        instances. The engine may keep processing afterwards. */
    void unload (AudioEngine::AudioEngine& engine) noexcept;

    //==============================================================================
    // Inspection

    /** One entry per plugin instrument slot from the last `loadInto()`. */
    const std::vector<SlotBinding>& getBindings() const noexcept { return bindingList; }

    /** The live instance for a slot, or nullptr when the slot is not a plugin
        slot owned by this store or is a placeholder. */
    plugins::PluginInstrument* getInstrument (int slotIndex) const noexcept;

    bool isPlaceholder (int slotIndex) const noexcept;

    //==============================================================================
    // State chunks

    /** Captures a live instance into a chunk.

        `uuid` is used when non-empty; otherwise a fresh UUID is generated.
        The chunk records the format, the identifier returned by
        `PluginDescription::createIdentifierString()`, name, manufacturer and
        the plugin's version stamp, so it can be matched on the next load.
    */
    static PluginStateChunk captureState (const plugins::PluginInstrument& instrument,
                                          std::string uuid = {});

    /** Default matching used by `loadInto()`: format, then plugin id, then
        name/manufacturer. Exposed so callers can diagnose a miss. */
    static bool findPlugin (const std::vector<juce::PluginDescription>& descriptions,
                            const PluginStateChunk& chunk,
                            juce::PluginDescription& match);

private:
    struct BoundSlot
    {
        SlotBinding info;
        std::unique_ptr<plugins::PluginInstrument> instrument;
        std::unique_ptr<AudioEngine::SlotSource> placeholder;
    };

    void releaseAll (AudioEngine::AudioEngine* engine) noexcept;

    bool resolve (const PluginStateChunk& chunk, juce::PluginDescription& description) const;

    juce::AudioPluginFormatManager formatManager;
    std::vector<juce::PluginDescription> availablePlugins;
    PluginResolver resolver;

    std::map<int, BoundSlot> boundSlots;
    std::vector<SlotBinding> bindingList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginStateStore)
};

} // namespace ihub::project
