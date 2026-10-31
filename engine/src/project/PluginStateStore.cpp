#include "PluginStateStore.h"

#include <algorithm>
#include <utility>

namespace ihub::project
{

namespace
{

/** A tiny SlotSource that keeps a slot alive but silent.

    It is used when a set references a plugin that is not installed. The slot
    stays in the graph (and in the UI) with the state id and plugin identity
    from the set, but renders silence instead of crashing or dereferencing a
    missing instance.
*/
class SilentPlaceholder final : public AudioEngine::SlotSource
{
public:
    SilentPlaceholder() = default;

    void prepare (double, int) override {}

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.clear();
    }

    int latencySamples() const noexcept override { return 0; }
};

juce::String describeSlot (const SlotConfig& slot)
{
    if (slot.name.empty())
        return "slot " + juce::String (slot.index);

    return "slot " + juce::String (slot.index) + " \"" + juce::String (slot.name) + "\"";
}

std::string nonEmptyOr (const juce::String& value, const char* fallback)
{
    return value.isNotEmpty() ? value.toStdString() : std::string (fallback);
}

bool nameMatches (const juce::PluginDescription& description, const PluginStateChunk& chunk)
{
    if (chunk.name.empty())
        return false;

    if (! description.name.equalsIgnoreCase (juce::String (chunk.name)))
        return false;

    if (chunk.manufacturer.empty())
        return true;

    return description.manufacturerName.equalsIgnoreCase (juce::String (chunk.manufacturer));
}

bool identifierMatches (const juce::PluginDescription& description, const std::string& id)
{
    if (id.empty())
        return false;

    const auto value = juce::String (id);

    if (value.equalsIgnoreCase (description.createIdentifierString()))
        return true;

    if (value.equalsIgnoreCase (description.fileOrIdentifier))
        return true;

    if (value == juce::String (description.uniqueId))
        return true;

    if (value.equalsIgnoreCase (juce::String::toHexString (description.uniqueId)))
        return true;

    return false;
}

} // namespace

const char* toString (PluginLoadWarningKind kind) noexcept
{
    switch (kind)
    {
        case PluginLoadWarningKind::missingPlugin: return "missing-plugin";
        case PluginLoadWarningKind::missingState:  return "missing-state";
        case PluginLoadWarningKind::loadFailed:    return "load-failed";
        case PluginLoadWarningKind::stateRejected: return "state-rejected";
    }

    return "missing-plugin";
}

int PluginLoadReport::getNumLoaded() const noexcept
{
    return static_cast<int> (std::count_if (bindings.begin(), bindings.end(),
                                            [] (const SlotBinding& binding)
                                            {
                                                return ! binding.placeholder;
                                            }));
}

int PluginLoadReport::getNumPlaceholders() const noexcept
{
    return static_cast<int> (std::count_if (bindings.begin(), bindings.end(),
                                            [] (const SlotBinding& binding)
                                            {
                                                return binding.placeholder;
                                            }));
}

juce::String PluginLoadReport::toString() const
{
    juce::String text;

    if (! success)
        return "load failed: " + error;

    text << "loaded " << getNumLoaded() << " plugin slot(s), "
         << getNumPlaceholders() << " placeholder(s)";

    for (const auto& warning : warnings)
        text << "\n  warning [" << ihub::project::toString (warning.kind) << "]: " << warning.message;

    return text;
}

//==============================================================================
PluginStateStore::PluginStateStore()
{
    // The same headless format set PluginInstrument uses. Instantiation still
    // happens in this process; scanning/crash isolation is the Scanner's job.
    juce::addHeadlessDefaultFormatsToManager (formatManager);
}

PluginStateStore::~PluginStateStore()
{
    // A Slot only borrows the source. The caller must call unload() (or keep
    // the engine stopped and clear its sources) before this object dies. The
    // assertion catches a lifetime bug in development builds.
    jassert (boundSlots.empty());
    releaseAll (nullptr);
}

void PluginStateStore::setAvailablePlugins (std::vector<juce::PluginDescription> descriptions)
{
    availablePlugins = std::move (descriptions);
}

void PluginStateStore::setResolver (PluginResolver newResolver)
{
    resolver = std::move (newResolver);
}

void PluginStateStore::clearResolver()
{
    resolver = nullptr;
}

bool PluginStateStore::resolve (const PluginStateChunk& chunk,
                                juce::PluginDescription& description) const
{
    if (resolver != nullptr)
        return resolver (chunk, description);

    return findPlugin (availablePlugins, chunk, description);
}

bool PluginStateStore::findPlugin (const std::vector<juce::PluginDescription>& descriptions,
                                   const PluginStateChunk& chunk,
                                   juce::PluginDescription& match)
{
    for (const auto& description : descriptions)
    {
        if (chunk.format.empty()
            || description.pluginFormatName.equalsIgnoreCase (juce::String (chunk.format)))
        {
            if (! chunk.pluginId.empty())
            {
                if (identifierMatches (description, chunk.pluginId))
                {
                    match = description;
                    return true;
                }

                continue;
            }

            if (nameMatches (description, chunk))
            {
                match = description;
                return true;
            }
        }
    }

    return false;
}

//==============================================================================
void PluginStateStore::releaseAll (AudioEngine::AudioEngine* engine) noexcept
{
    if (engine != nullptr)
    {
        for (const auto& entry : boundSlots)
        {
            if (entry.first < 0 || entry.first >= engine->getNumSlots())
                continue;

            auto& slot = engine->getSlot (entry.first);

            // Only detach a source that this store installed. A slot whose
            // source changed underneath us was handed to someone else and is
            // not ours to clear.
            if (entry.second.instrument != nullptr
                && slot.getSource() == entry.second.instrument.get())
            {
                slot.setSource (nullptr);
            }
            else if (entry.second.placeholder != nullptr
                     && slot.getSource() == entry.second.placeholder.get())
            {
                slot.setSource (nullptr);
            }
        }
    }

    boundSlots.clear();
    bindingList.clear();
}

PluginLoadReport PluginStateStore::loadInto (const SetFile& set,
                                             AudioEngine::AudioEngine& engine,
                                             double sampleRate,
                                             int maximumBlockSize)
{
    PluginLoadReport report;

    juce::String error;

    // Applies gains, mute and activation, and validates slot count, indices
    // and every plugin state reference first. A structurally invalid set is
    // the only fatal case: it is not safe to guess which slots to rewire.
    if (! set.applyTo (engine, error))
    {
        report.success = false;
        report.error = error;
        report.bindings = bindingList;
        return report;
    }

    // Drop the previous binding before assigning new sources. The stream must
    // be stopped (see the header); that is what makes this safe.
    releaseAll (&engine);

    const auto rate = engine.isPrepared() && engine.getSampleRate() > 0.0
        ? engine.getSampleRate()
        : (sampleRate > 0.0 ? sampleRate : 48000.0);
    const auto blockSize = engine.isPrepared() && engine.getMaximumBlockSize() > 0
        ? engine.getMaximumBlockSize()
        : (maximumBlockSize > 0 ? maximumBlockSize : 512);

    for (const auto& slotConfig : set.slots)
    {
        if (slotConfig.instrument.kind != SourceKind::plugin)
            continue;

        // Lazy patch state (T119): a plugin patch with no blob has never been
        // loaded or edited. It is a descriptor only, so it stays out of the
        // graph until something warms it, instead of earning a permanent
        // "missing state" placeholder.
        if (slotConfig.instrument.pluginState.empty())
            continue;

        auto& slot = engine.getSlot (slotConfig.index);
        const auto context = describeSlot (slotConfig);

        SlotBinding info;
        info.slotIndex = slotConfig.index;
        info.pluginStateId = slotConfig.instrument.pluginState;
        info.pluginName = slotConfig.instrument.name;

        const auto* chunk = set.findPluginState (slotConfig.instrument.pluginState);

        const auto installPlaceholder = [&] (PluginLoadWarningKind kind,
                                             const juce::String& message)
        {
            PluginLoadWarning warning;
            warning.kind = kind;
            warning.slotIndex = slotConfig.index;
            warning.pluginStateId = info.pluginStateId;
            warning.pluginId = info.pluginId;
            warning.pluginName = info.pluginName;
            warning.message = message;

            report.warnings.push_back (warning);

            info.placeholder = true;
            info.warning = message;

            auto placeholder = std::make_unique<SilentPlaceholder>();
            slot.setSource (placeholder.get());

            BoundSlot bound;
            bound.info = info;
            bound.placeholder = std::move (placeholder);
            boundSlots[slotConfig.index] = std::move (bound);
            bindingList.push_back (info);
        };

        if (chunk == nullptr)
        {
            info.pluginName = slotConfig.instrument.name.empty()
                ? slotConfig.instrument.pluginState
                : slotConfig.instrument.name;

            installPlaceholder (PluginLoadWarningKind::missingState,
                                context + ": missing plugin state \""
                                    + juce::String (info.pluginStateId)
                                    + "\"; using a silent placeholder");
            continue;
        }

        info.pluginId = chunk->pluginId;
        info.pluginName = chunk->name.empty() ? slotConfig.instrument.name : chunk->name;
        info.pluginVersion = chunk->version;

        juce::PluginDescription description;

        if (! resolve (*chunk, description))
        {
            installPlaceholder (PluginLoadWarningKind::missingPlugin,
                                context + ": missing plugin \"" + juce::String (info.pluginName)
                                    + "\" (" + juce::String (info.pluginId)
                                    + "); using a silent placeholder");
            continue;
        }

        auto instrument = std::make_unique<plugins::PluginInstrument>();
        juce::String loadError;

        if (! instrument->load (formatManager, description, rate, blockSize, loadError))
        {
            installPlaceholder (PluginLoadWarningKind::loadFailed,
                                context + ": plugin \"" + juce::String (info.pluginName)
                                    + "\" could not be loaded: "
                                    + (loadError.isEmpty() ? juce::String ("unknown error")
                                                           : loadError)
                                    + "; using a silent placeholder");
            continue;
        }

        bool stateRestored = true;

        // An empty blob is a valid "factory default" state for a plugin; only
        // a non-empty blob that the plugin rejects is worth a warning. The
        // instance is kept either way, so the slot still makes sound.
        if (chunk->data.getSize() > 0)
            stateRestored = instrument->setStateInformation (chunk->data);

        if (! stateRestored)
        {
            PluginLoadWarning warning;
            warning.kind = PluginLoadWarningKind::stateRejected;
            warning.slotIndex = slotConfig.index;
            warning.pluginStateId = info.pluginStateId;
            warning.pluginId = info.pluginId;
            warning.pluginName = info.pluginName;
            warning.message = context + ": plugin state \""
                + juce::String (info.pluginStateId)
                + "\" was rejected; the plugin was loaded with its defaults";

            report.warnings.push_back (warning);
            info.warning = warning.message;
        }

        instrument->prepare (rate, blockSize);
        slot.setSource (instrument.get());

        BoundSlot bound;
        bound.info = info;
        bound.instrument = std::move (instrument);
        boundSlots[slotConfig.index] = std::move (bound);
        bindingList.push_back (info);
    }

    report.bindings = bindingList;
    return report;
}

void PluginStateStore::unload (AudioEngine::AudioEngine& engine) noexcept
{
    releaseAll (&engine);
}

plugins::PluginInstrument* PluginStateStore::getInstrument (int slotIndex) const noexcept
{
    const auto found = boundSlots.find (slotIndex);

    if (found == boundSlots.end())
        return nullptr;

    return found->second.instrument.get();
}

bool PluginStateStore::isPlaceholder (int slotIndex) const noexcept
{
    const auto found = boundSlots.find (slotIndex);

    return found != boundSlots.end() && found->second.info.placeholder;
}

//==============================================================================
PluginStateChunk PluginStateStore::captureState (const plugins::PluginInstrument& instrument,
                                                  std::string uuid)
{
    PluginStateChunk chunk;

    chunk.uuid = uuid.empty() ? makePluginStateId() : std::move (uuid);

    const auto& description = instrument.getPluginDescription();

    chunk.format = nonEmptyOr (description.pluginFormatName, "VST3");

    // createIdentifierString() is JUCE's stable, human-readable identity:
    // format + name + unique id. It is the primary key findPlugin() uses.
    chunk.pluginId = description.createIdentifierString().toStdString();

    if (chunk.pluginId.empty())
        chunk.pluginId = description.name.toStdString();

    chunk.name = description.name.isNotEmpty() ? description.name.toStdString()
                                               : instrument.getName().toStdString();
    chunk.manufacturer = description.manufacturerName.toStdString();

    // The version stamp is what later migrations use to decide whether an
    // opaque blob can still be handed to this plugin build.
    chunk.version = nonEmptyOr (description.version, "unknown");

    chunk.data = instrument.getStateInformation();
    return chunk;
}

} // namespace ihub::project
