#include "Server.h"
#include "Routes.h"
#include "StateBroadcast.h"

#include <httplib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <thread>
#include <utility>

namespace Api
{

namespace
{
constexpr int minimumMeterHz = 1;
constexpr int maximumMeterHz = 120;

int clampMeterHz (int hz) noexcept
{
    return juce::jlimit (minimumMeterHz, maximumMeterHz, hz);
}

bool isLoopbackAddress (const std::string& address)
{
    return address == "127.0.0.1"
        || address == "::1"
        || address == "[::1]"
        || address == "::ffff:127.0.0.1"
        || address.rfind ("127.", 0) == 0;
}

/** Linear gain for a patch volume. -60 dB is the documented silence floor. */
float patchGainFromDb (double volumeDb) noexcept
{
    if (volumeDb <= -60.0)
        return 0.0f;

    return static_cast<float> (std::pow (10.0, volumeDb / 20.0));
}
} // namespace

//==============================================================================
// StateStore
//==============================================================================

StateStore::StateStore (int numSlotsToUse)
    : numSlots (juce::jmax (1, numSlotsToUse)),
      startedAt (std::chrono::steady_clock::now())
{
    slotNames.reserve (static_cast<std::size_t> (numSlots));
    slotFamilies.assign (static_cast<std::size_t> (numSlots), {});
    slotFamilyNames.assign (static_cast<std::size_t> (numSlots), {});
    slotPatches.assign (static_cast<std::size_t> (numSlots), {});
    slotPatchNames.assign (static_cast<std::size_t> (numSlots), {});
    slotLatencySamples.assign (static_cast<std::size_t> (numSlots), 0);
    slotLatencyMs.assign (static_cast<std::size_t> (numSlots), 0.0);
    slotInstruments.assign (static_cast<std::size_t> (numSlots), {});
    slotMixes.assign (static_cast<std::size_t> (numSlots), {});
    slotEditorOpen.assign (static_cast<std::size_t> (numSlots), false);

    for (int index = 0; index < numSlots; ++index)
    {
        slotNames.push_back ("Slot " + std::to_string (index + 1));
        slotInstruments[static_cast<std::size_t> (index)].name = slotNames.back();
    }
}

int StateStore::getArmedSlot() const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);
    return armedSlot;
}

bool StateStore::setArmedSlot (int slot)
{
    ArmHandler handler;
    ChangeHandler changed;
    int appliedSlot = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot >= numSlots)
            return false;

        appliedSlot = slot < 0 ? -1 : slot;
        armedSlot = appliedSlot;
        handler = armHandler;
        changed = changeHandler;
    }

    if (handler != nullptr)
        handler (appliedSlot);

    if (changed != nullptr)
        changed ("arm", appliedSlot);

    return true;
}

void StateStore::setArmHandler (ArmHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    armHandler = std::move (handler);
}

SettingsState StateStore::getSettings() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return settings;
}

bool StateStore::updateSettings (const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        auto next = settings;
        const auto& properties = patch.getDynamicObject()->getProperties();

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);

            if (name.toString() == "masterGain")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "masterGain", 0.0, 1.5, parsed, error))
                    return false;

                next.masterGain = parsed;
            }
            else if (name.toString() == "inputGain")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "inputGain", 0.0, 2.0, parsed, error))
                    return false;

                next.inputGain = parsed;
            }
            else if (name.toString() == "midiChannel")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "midiChannel", 1, 16, parsed, error))
                    return false;

                next.midiChannel = static_cast<int> (parsed);
            }
            else if (name.toString() == "metronome")
            {
                bool parsed = false;

                if (! detail::readBool (value, "metronome", parsed, error))
                    return false;

                next.metronome = parsed;
            }
            else
            {
                error = "unknown setting: " + name.toString().toStdString();
                return false;
            }
        }

        settings = next;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("settings", -1);

    return true;
}

TransportState StateStore::getTransport() const
{
    TransportProvider provider;
    TransportState current;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        current = transport;
        provider = transportProvider;
    }

    if (provider != nullptr)
    {
        try
        {
            return provider();
        }
        catch (...)
        {
            // A broken provider must not take the HTTP thread down.
        }
    }

    return current;
}

bool StateStore::updateTransport (const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    TransportHandler handler;
    ChangeHandler changed;
    TransportState next;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        next = transport;
        const auto& properties = patch.getDynamicObject()->getProperties();

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);

            if (name.toString() == "playing")
            {
                if (! detail::readBool (value, "playing", next.playing, error))
                    return false;
            }
            else if (name.toString() == "bpm")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "bpm", 20.0, 300.0, parsed, error))
                    return false;

                next.bpm = parsed;
            }
            else if (name.toString() == "ppq")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "ppq", 0.0, 1.0e9, parsed, error))
                    return false;

                next.ppq = parsed;
            }
            else if (name.toString() == "positionSamples")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "positionSamples",
                                   0, std::numeric_limits<std::int64_t>::max(),
                                   parsed, error))
                {
                    return false;
                }

                next.positionSamples = parsed;
            }
            else
            {
                error = "unknown transport field: " + name.toString().toStdString();
                return false;
            }
        }

        transport = next;
        handler = transportHandler;
        changed = changeHandler;
    }

    if (handler != nullptr)
        handler (next, patch);

    if (changed != nullptr)
        changed ("transport", -1);

    return true;
}

void StateStore::setTransportHandler (TransportHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    transportHandler = std::move (handler);
}

void StateStore::setTransportProvider (TransportProvider provider)
{
    const std::lock_guard<std::mutex> lock (mutex);
    transportProvider = std::move (provider);
}

void StateStore::setSlotRuntimeProvider (SlotRuntimeProvider provider)
{
    const std::lock_guard<std::mutex> lock (mutex);
    slotRuntimeProvider = std::move (provider);
}

void StateStore::setChangeHandler (ChangeHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    changeHandler = std::move (handler);
}

int StateStore::getNumSlots() const noexcept
{
    return numSlots;
}

void StateStore::setSlotName (int slot, std::string name)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
            return;

        slotNames[static_cast<std::size_t> (slot)] = std::move (name);
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", slot);
}

void StateStore::setSlotIdentity (int slot,
                                  std::string family,
                                  std::string familyName,
                                  std::string patch,
                                  std::string patchName)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
            return;

        const auto index = static_cast<std::size_t> (slot);
        slotFamilies[index] = std::move (family);
        slotFamilyNames[index] = std::move (familyName);
        slotPatches[index] = std::move (patch);
        slotPatchNames[index] = std::move (patchName);
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", slot);
}

void StateStore::setFamilyTree (std::vector<FamilyState> tree)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        families = std::move (tree);
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", -1);
}

std::vector<FamilyState> StateStore::getFamilyTree() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return families;
}

//==============================================================================
// Patch mix / colour (T120)

PatchState StateStore::makePatchLocked (int slot) const
{
    PatchState patch;
    patch.slot = slot;

    if (slot < 0 || slot >= numSlots)
        return patch;

    const auto index = static_cast<std::size_t> (slot);
    patch.id = slotPatches[index];
    patch.name = slotPatchNames[index].empty() ? slotNames[index]
                                               : slotPatchNames[index];
    patch.family = slotFamilies[index];
    patch.familyName = slotFamilyNames[index];
    patch.instrument = slotInstruments[index];

    if (patch.instrument.name.empty())
        patch.instrument.name = patch.name;

    const auto& mix = slotMixes[index];
    patch.volumeDb = mix.volumeDb;
    patch.pan = mix.pan;
    patch.mute = mix.mute;
    patch.solo = mix.solo;
    patch.colorIndex = mix.colorIndex;
    patch.editorOpen = slotEditorOpen[index];

    return patch;
}

void StateStore::setPatchInstrument (int slot, PatchInstrumentState instrument)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (slot < 0 || slot >= numSlots)
        return;

    slotInstruments[static_cast<std::size_t> (slot)] = std::move (instrument);
}

void StateStore::setPatchMix (int slot,
                              double volumeDb,
                              double pan,
                              bool mute,
                              bool solo,
                              int colorIndex)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (slot < 0 || slot >= numSlots)
        return;

    auto& mix = slotMixes[static_cast<std::size_t> (slot)];
    mix.volumeDb = juce::jlimit (-60.0, 12.0, volumeDb);
    mix.pan = juce::jlimit (-1.0, 1.0, pan);
    mix.mute = mute;
    mix.solo = solo;
    mix.colorIndex = juce::jlimit (0, 15, colorIndex);
}

PatchState StateStore::getPatch (int slot) const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return makePatchLocked (slot);
}

void StateStore::collectEffectiveGainsLocked (const std::vector<PatchMixState>& mixes,
                                              std::vector<float>& gains) const
{
    bool anySolo = false;

    for (const auto& mix : mixes)
        anySolo = anySolo || mix.solo;

    gains.assign (mixes.size(), 0.0f);

    for (std::size_t index = 0; index < mixes.size(); ++index)
    {
        const auto& mix = mixes[index];

        if (mix.mute || (anySolo && ! mix.solo))
            continue;

        gains[index] = juce::jlimit (0.0f, 4.0f,
                                     patchGainFromDb (mix.volumeDb));
    }
}

bool StateStore::updatePatch (int slot, const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    ChangeHandler changed;
    PatchHandler applyPatch;
    std::vector<PatchState> patches;
    std::vector<float> gains;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
        {
            error = "slot is out of range";
            return false;
        }

        auto next = slotMixes[static_cast<std::size_t> (slot)];
        const auto& properties = patch.getDynamicObject()->getProperties();

        if (properties.isEmpty())
        {
            error = "body must contain at least one patch field";
            return false;
        }

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);
            const auto key = name.toString();

            if (key == "volumeDb")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "volumeDb", -60.0, 12.0,
                                          parsed, error))
                    return false;

                next.volumeDb = parsed;
            }
            else if (key == "pan")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "pan", -1.0, 1.0, parsed, error))
                    return false;

                next.pan = parsed;
            }
            else if (key == "mute" || key == "muted")
            {
                bool parsed = false;

                if (! detail::readBool (value, key.toRawUTF8(), parsed, error))
                    return false;

                next.mute = parsed;
            }
            else if (key == "solo")
            {
                bool parsed = false;

                if (! detail::readBool (value, "solo", parsed, error))
                    return false;

                next.solo = parsed;
            }
            else if (key == "colorIndex")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "colorIndex", 0, 15,
                                           parsed, error))
                    return false;

                next.colorIndex = static_cast<int> (parsed);
            }
            else
            {
                error = "unknown patch field: " + key.toStdString();
                return false;
            }
        }

        slotMixes[static_cast<std::size_t> (slot)] = next;

        collectEffectiveGainsLocked (slotMixes, gains);
        patches.reserve (static_cast<std::size_t> (numSlots));

        for (int index = 0; index < numSlots; ++index)
            patches.push_back (makePatchLocked (index));

        changed = changeHandler;
        applyPatch = patchHandler;
    }

    if (applyPatch != nullptr)
    {
        for (int index = 0; index < numSlots; ++index)
            applyPatch (index, patches[static_cast<std::size_t> (index)],
                        gains[static_cast<std::size_t> (index)]);
    }

    if (changed != nullptr)
        changed ("patch", slot);

    return true;
}

void StateStore::setPatchHandler (PatchHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchHandler = std::move (handler);
}

void StateStore::setPatchSourceHandler (PatchSourceHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchSourceHandler = std::move (handler);
}

bool StateStore::applyPatchSource (int slot,
                                   const PatchSourceRequest& source,
                                   std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    if (source.kind != "empty"
        && source.kind != "init-tone"
        && source.kind != "sampler"
        && source.kind != "plugin"
        && source.kind != "audio-input")
    {
        error = "kind must be empty, init-tone, sampler, plugin or audio-input";
        return false;
    }

    PatchSourceHandler applySource;
    PatchInstrumentState previous;
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        applySource = patchSourceHandler;
        previous = slotInstruments[static_cast<std::size_t> (slot)];
    }

    if (applySource == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    if (! applySource (slot, source, error))
    {
        // The live swap refused the request, so the descriptor stays as it was.
        return false;
    }

    PatchInstrumentState next;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        next = slotInstruments[static_cast<std::size_t> (slot)];
        next.kind = source.kind;
        next.id = previous.id;

        if (! source.name.empty())
            next.name = source.name;
        else if (! source.pluginName.empty())
            next.name = source.pluginName;
        else if (next.name.empty())
            next.name = slotPatches[static_cast<std::size_t> (slot)];

        // A source switch replaces the descriptor wholesale: a plug-in patch
        // must not keep the previous kit path, and a tone must not keep a
        // plug-in identity.
        next.pluginFormat.clear();
        next.pluginId.clear();
        next.pluginName.clear();
        next.pluginManufacturer.clear();
        next.pluginVersion.clear();
        next.samplePath.clear();

        if (source.kind == "plugin")
        {
            next.pluginFormat = source.pluginFormat.empty() ? "VST3" : source.pluginFormat;
            next.pluginId = source.pluginId;
            next.pluginName = source.pluginName.empty() ? source.name : source.pluginName;
            next.pluginManufacturer = source.pluginManufacturer;
            next.pluginVersion = source.pluginVersion;
        }
        else if (source.kind == "sampler")
        {
            next.samplePath = source.samplePath.empty() ? source.path : source.samplePath;
        }

        slotInstruments[static_cast<std::size_t> (slot)] = next;
        // The session closes the old editor before the source is retired.
        slotEditorOpen[static_cast<std::size_t> (slot)] = false;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("patch", slot);

    return true;
}

//==============================================================================
// Native plugin editor (T129)

void StateStore::setPatchEditorHandlers (PatchEditorHandler openEditor,
                                         PatchEditorHandler closeEditor)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchEditorHandler = std::move (openEditor);
    patchEditorCloseHandler = std::move (closeEditor);
}

bool StateStore::openPatchEditor (int slot, std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    PatchEditorHandler open;
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        open = patchEditorHandler;
    }

    if (open == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    std::string handlerError;

    if (! open (slot, handlerError))
    {
        error = handlerError.empty() ? "could not open the plugin editor" : handlerError;
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (mutex);
        slotEditorOpen[static_cast<std::size_t> (slot)] = true;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("patch", slot);

    error.clear();
    return true;
}

bool StateStore::closePatchEditor (int slot, std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    PatchEditorHandler close;
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        close = patchEditorCloseHandler;
    }

    if (close == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    std::string handlerError;

    if (! close (slot, handlerError))
    {
        error = handlerError.empty() ? "could not close the plugin editor" : handlerError;
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (mutex);
        slotEditorOpen[static_cast<std::size_t> (slot)] = false;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("patch", slot);

    error.clear();
    return true;
}

//==============================================================================
// Per-patch insert chain (T121)

void StateStore::setFxHandlers (FxChainProvider provider, FxCommandHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    fxProvider = std::move (provider);
    fxHandler = std::move (handler);
}

bool StateStore::getFxChain (int slot, FxChainState& chain, std::string& error) const
{
    FxChainProvider provider;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        provider = fxProvider;
    }

    if (provider == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    try
    {
        return provider (slot, chain, error);
    }
    catch (...)
    {
        error = "the FX chain reader failed";
        return false;
    }
}

bool StateStore::applyFxCommand (int slot,
                                 const FxCommand& command,
                                 FxChainState& chain,
                                 std::string& error)
{
    FxCommandHandler handler;
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        handler = fxHandler;
        changed = changeHandler;
    }

    if (handler == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    bool applied = false;

    try
    {
        applied = handler (slot, command, chain, error);
    }
    catch (...)
    {
        error = "the FX chain edit failed";
        return false;
    }

    if (applied && changed != nullptr)
        changed ("patch", slot);

    return applied;
}

void StateStore::setSlotLatency (int slot, int samples, double sampleRate)
{
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
            return;

        const int clampedSamples = juce::jmax (0, samples);
        const auto index = static_cast<std::size_t> (slot);

        slotLatencySamples[index] = clampedSamples;
        slotLatencyMs[index] = sampleRate > 0.0
                                   ? static_cast<double> (clampedSamples) * 1000.0 / sampleRate
                                   : 0.0;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("slot", slot);
}

StateSnapshot StateStore::snapshot() const
{
    StateSnapshot result;
    TransportProvider transportReader;
    SlotRuntimeProvider slotReader;
    std::vector<PatchInstrumentState> instruments;
    std::vector<PatchMixState> mixes;
    std::vector<bool> editors;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        result.armedSlot = armedSlot;
        result.settings = settings;
        result.transport = transport;
        result.slots.reserve (static_cast<std::size_t> (numSlots));

        for (int index = 0; index < numSlots; ++index)
        {
            const auto position = static_cast<std::size_t> (index);

            SlotSnapshot slot;
            slot.index = index;
            slot.name = slotNames[position];
            slot.armed = index == armedSlot;
            slot.family = slotFamilies[position];
            slot.familyName = slotFamilyNames[position];
            slot.patch = slotPatches[position];
            slot.patchName = slotPatchNames[position];
            slot.latencySamples = slotLatencySamples[position];
            slot.latencyMs = slotLatencyMs[position];
            result.slots.push_back (std::move (slot));
        }

        result.families = families;
        instruments = slotInstruments;
        mixes = slotMixes;
        editors = slotEditorOpen;

        transportReader = transportProvider;
        slotReader = slotRuntimeProvider;
    }

    // The providers read the live session. They run outside the store's lock
    // and only touch lock-free audio-graph values.
    if (transportReader != nullptr)
    {
        try
        {
            result.transport = transportReader();
        }
        catch (...)
        {
        }
    }

    if (slotReader != nullptr)
    {
        for (auto& slot : result.slots)
        {
            try
            {
                const auto runtime = slotReader (slot.index);
                slot.armed = runtime.armed;
                slot.gain = runtime.gain;
                slot.state = runtime.state;
                slot.warmState = runtime.warmState;
                slot.warm = runtime.warm;
                slot.editorOpen = runtime.editorOpen;
            }
            catch (...)
            {
                // Keep the store's synthetic values for this slot.
            }
        }
    }

    // Merge the per-patch state and the live slot values into the family tree.
    for (auto& family : result.families)
    {
        for (auto& patch : family.patches)
        {
            const int slot = patch.slot;

            if (slot < 0 || slot >= static_cast<int> (mixes.size())
                || slot >= static_cast<int> (result.slots.size()))
                continue;

            const auto index = static_cast<std::size_t> (slot);
            const auto& mix = mixes[index];
            const auto& live = result.slots[index];

            patch.instrument = instruments[index];
            patch.volumeDb = mix.volumeDb;
            patch.pan = mix.pan;
            patch.mute = mix.mute;
            patch.solo = mix.solo;
            patch.colorIndex = mix.colorIndex;

            if (patch.instrument.name.empty())
                patch.instrument.name = patch.name;

            patch.warmState = live.warmState;
            patch.warm = live.warm;
            patch.editorOpen = slotReader != nullptr
                                   ? live.editorOpen
                                   : (index < editors.size() && editors[index]);
            patch.latencySamples = live.latencySamples;
            patch.latencyMs = live.latencyMs;
        }
    }

    result.uptimeMs = getUptimeMs();
    return result;
}

double StateStore::getUptimeMs() const noexcept
{
    const auto elapsed = std::chrono::steady_clock::now() - startedAt;
    return std::chrono::duration<double, std::milli> (elapsed).count();
}

//==============================================================================
// Server
//==============================================================================

struct Server::Impl
{
    Impl (StateStore& storeToUse, Options optionsToUse)
        : state (storeToUse),
          options (std::move (optionsToUse)),
          authToken (options.authToken),
          meterHz (clampMeterHz (options.meterHz)),
          broadcast (storeToUse),
          pairing ({}, options.port),
          nextPort (options.port)
    {
        if (options.lanMode)
            pairing.setLanEnabled (true);
    }

    bool isLoopbackRequest (const httplib::Request& request) const
    {
        return isLoopbackAddress (request.remote_addr);
    }

    bool authorized (const httplib::Request& request) const
    {
        // Exchanging a pairing code is how a client obtains its first token, so
        // it cannot require one. The management endpoints expose the live code
        // and are therefore loopback-only, even for an already paired client:
        // a tablet must not be able to mint more tokens for itself.
        if (request.path == "/pair")
            return true;

        if (request.path.rfind ("/pairing", 0) == 0)
            return isLoopbackRequest (request);

        std::string expected;
        bool lanEnabled = false;

        {
            const std::lock_guard<std::mutex> lock (configMutex);
            expected = authToken;
            lanEnabled = pairing.isLanEnabled();
        }

        if (expected.empty() && ! lanEnabled)
            return true;

        auto supplied = request.get_header_value ("X-Instrument-Hub-Token");

        if (supplied.empty())
        {
            const auto authorization = request.get_header_value ("Authorization");
            constexpr const char* bearer = "Bearer ";

            if (authorization.rfind (bearer, 0) == 0)
                supplied = authorization.substr (std::char_traits<char>::length (bearer));
        }

        if (! expected.empty() && supplied == expected)
            return true;

        return lanEnabled && pairing.validateToken (supplied);
    }

    /** The address the listener should be bound to right now. */
    std::string desiredAddress() const
    {
        return pairing.isLanEnabled() ? "0.0.0.0" : options.address;
    }

    MeterFrame makeDefaultFrame() const
    {
        const auto snapshot = state.snapshot();

        MeterFrame frame;
        frame.timestampMs = snapshot.uptimeMs;
        frame.slots.reserve (snapshot.slots.size());

        for (const auto& slot : snapshot.slots)
        {
            SlotMeter meter;
            meter.slot = slot.index;
            meter.armed = slot.armed;
            meter.gain = slot.armed ? 1.0f : 0.0f;
            frame.slots.push_back (meter);
        }

        return frame;
    }

    void meterLoop()
    {
        const auto interval = std::chrono::milliseconds (1000 / meterHz);
        auto next = std::chrono::steady_clock::now();
        std::uint64_t sequence = 0;

        while (running.load (std::memory_order_acquire))
        {
            MeterProvider provider;

            {
                const std::lock_guard<std::mutex> lock (configMutex);
                provider = meterProvider;
            }

            MeterFrame frame;

            if (provider != nullptr)
            {
                try
                {
                    frame = provider();
                }
                catch (...)
                {
                    frame = makeDefaultFrame();
                }
            }
            else
            {
                frame = makeDefaultFrame();
            }

            frame.sequence = ++sequence;

            if (! (frame.timestampMs > 0.0))
                frame.timestampMs = state.getUptimeMs();

            auto json = toJson (meterFrameToVar (frame));

            {
                const std::lock_guard<std::mutex> lock (frameMutex);
                latestFrameJson = std::move (json);
                latestFrameVersion = sequence;
            }

            next += interval;
            const auto now = std::chrono::steady_clock::now();

            if (next > now)
                std::this_thread::sleep_until (next);
            else
                next = now;
        }
    }

    void registerMeterRoute()
    {
        http->WebSocket ("/meters", [this] (const httplib::Request& request,
                                            httplib::ws::WebSocket& websocket)
        {
            if (! authorized (request))
            {
                websocket.close (httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
                return;
            }

            activeWebSockets.fetch_add (1, std::memory_order_acq_rel);

            // Short read timeout: the loop must stay responsive to new meter
            // frames while still reading so ping/pong and closes are handled.
            websocket.set_read_timeout (0, 10000);

            std::uint64_t sentVersion = 0;
            std::string incoming;

            while (running.load (std::memory_order_acquire) && websocket.is_open())
            {
                std::string payload;
                std::uint64_t version = 0;

                {
                    const std::lock_guard<std::mutex> lock (frameMutex);
                    version = latestFrameVersion;

                    if (version != sentVersion)
                        payload = latestFrameJson;
                }

                if (version != sentVersion)
                {
                    if (! websocket.send (payload))
                        break;

                    sentVersion = version;
                    meterFramesSent.fetch_add (1, std::memory_order_relaxed);
                }

                const auto result = websocket.read (incoming);

                if (result == httplib::ws::ReadResult::Fail)
                    break;
            }

            if (websocket.is_open())
                websocket.close (httplib::ws::CloseStatus::Normal, "server stopping");

            activeWebSockets.fetch_sub (1, std::memory_order_acq_rel);
        });
    }

    /** Serves `/events`: the coalesced live state mirror (T103). */
    void registerStateRoute()
    {
        http->WebSocket ("/events", [this] (const httplib::Request& request,
                                           httplib::ws::WebSocket& websocket)
        {
            if (! authorized (request))
            {
                websocket.close (httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
                return;
            }

            activeWebSockets.fetch_add (1, std::memory_order_acq_rel);
            broadcast.handleClient (websocket);
            activeWebSockets.fetch_sub (1, std::memory_order_acq_rel);
        });
    }

    /** Starts advertising the API on the LAN, replacing any old record set.

        Called after a successful bind and only when LAN mode is on. A failure
        (no multicast route, port 5353 taken by a non-sharing daemon) is logged
        and never stops the HTTP listener.

        Caller holds `lifecycleMutex`.
    */
    void startDiscoveryLocked()
    {
        DiscoveryService service;
        service.instanceName = "Instrument Hub";
        service.port = boundPort.load (std::memory_order_acquire);
        service.address = pairing.getAdvertisedAddress();

        if (service.address.rfind ("127.", 0) == 0)
            service.address.clear();

        if (! discovery.start (std::move (service)))
            std::cerr << "mdns: could not advertise " << Discovery::serviceType
                      << ": " << discovery.getLastError() << std::endl;
    }

    /** Creates the HTTP listener on the address LAN mode currently wants.

        Caller holds `lifecycleMutex`.
    */
    bool bindListenerLocked()
    {
        if (listening.load (std::memory_order_acquire))
            return true;

        http = std::make_unique<httplib::Server>();

        http->set_pre_routing_handler ([this] (const httplib::Request& request,
                                               httplib::Response& response)
        {
            // A meter or state WebSocket handler blocks for the life of the
            // connection, so it must not hold up a LAN rebind. Every other
            // request is counted until its response has been prepared.
            const bool webSocketUpgrade = request.path == "/meters"
                                          || request.path == "/events";

            if (! webSocketUpgrade)
                requestsInFlight.fetch_add (1, std::memory_order_acq_rel);

            if (rebinding.load (std::memory_order_acquire))
            {
                if (! webSocketUpgrade)
                    requestsInFlight.fetch_sub (1, std::memory_order_acq_rel);

                setError (response, 503, "the control API is switching LAN mode");
                return httplib::Server::HandlerResponse::Handled;
            }

            // Route handlers add the CORS headers through setJson()/setError();
            // only a pre-flight response needs them before a handler runs.
            if (request.method == "OPTIONS")
            {
                addCorsHeaders (response);
                response.status = 204;
                return httplib::Server::HandlerResponse::Handled;
            }

            if (! authorized (request))
            {
                setError (response, 401, "unauthorized");
                return httplib::Server::HandlerResponse::Handled;
            }

            return httplib::Server::HandlerResponse::Unhandled;
        });

        http->set_post_routing_handler ([this] (const httplib::Request& request,
                                                httplib::Response&)
        {
            if (request.path != "/meters" && request.path != "/events")
                requestsInFlight.fetch_sub (1, std::memory_order_acq_rel);
        });

        RouteContext context { state, {}, {}, {}, {}, &pairing,
                               {}, {}, {}, {}, {}, {} };

        {
            const std::lock_guard<std::mutex> lock (configMutex);
            context.audioProvider = audioProvider;
            context.audioApply = audioApply;
            context.audioMeasure = audioMeasure;
            context.pluginProvider = pluginProvider;
            context.pluginRescan = pluginRescan;
            context.midiProvider = midiProvider;
            context.midiEnableInput = midiEnableInput;
            context.midiPadTest = midiPadTest;
        }

        context.setLanMode = [this] (bool enabled) { return applyLanMode (enabled); };

        registerRoutes (*http, context);
        registerMeterRoute();
        registerStateRoute();

        const auto address = desiredAddress();
        int port = nextPort;

        if (port > 0)
        {
            if (! http->bind_to_port (address, port))
            {
                http.reset();
                return false;
            }
        }
        else
        {
            port = http->bind_to_any_port (address);

            if (port <= 0)
            {
                http.reset();
                return false;
            }
        }

        boundPort.store (port, std::memory_order_release);
        nextPort = port;
        listenerAddress = address;
        listening.store (true, std::memory_order_release);
        pairing.setEndpoint ({}, port);

        if (pairing.isLanEnabled())
            startDiscoveryLocked();

        listenerThread = std::thread ([this] { http->listen_after_bind(); });

        return true;
    }

    /** Stops the listener only; the meter/control threads keep running.

        Caller holds `lifecycleMutex`.
    */
    void stopListenerLocked()
    {
        discovery.stop();

        if (! listening.exchange (false, std::memory_order_acq_rel))
        {
            if (http != nullptr)
                http.reset();

            return;
        }

        if (http != nullptr)
            http->stop();

        if (listenerThread.joinable())
            listenerThread.join();

        http.reset();
        boundPort.store (0, std::memory_order_release);
    }

    /** Rebind after a LAN-mode change.

        Caller holds `lifecycleMutex`.
    */
    bool rebindLocked()
    {
        stopListenerLocked();
        return bindListenerLocked();
    }

    PairingSnapshot applyLanMode (bool enabled)
    {
        const auto changed = pairing.setLanEnabled (enabled);
        const auto snapshot = pairing.snapshot();

        if (changed)
        {
            {
                const std::lock_guard<std::mutex> lock (controlMutex);
                rebindRequested = true;
            }

            controlCv.notify_one();
        }

        return snapshot;
    }

    void waitForRequestsToDrain()
    {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::seconds (2);

        while (requestsInFlight.load (std::memory_order_acquire) > 0
               && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        // The response that triggered the toggle is written after the handler
        // returns; give the worker a moment to flush it before its server dies.
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    void controlLoop()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock (controlMutex);
            controlCv.wait (lock, [this]
            {
                return controlStop || rebindRequested;
            });

            if (controlStop)
                return;

            rebindRequested = false;
            lock.unlock();

            rebinding.store (true, std::memory_order_release);
            waitForRequestsToDrain();

            {
                const std::lock_guard<std::mutex> life (lifecycleMutex);

                if (running.load (std::memory_order_acquire))
                    rebindLocked();
            }

            rebinding.store (false, std::memory_order_release);
        }
    }

    StateStore& state;
    const Options options;

    std::unique_ptr<httplib::Server> http;
    std::thread listenerThread;
    std::thread meterThread;
    std::thread controlThread;

    std::atomic<bool> running { false };
    std::atomic<bool> listening { false };
    std::atomic<bool> rebinding { false };
    std::atomic<int> requestsInFlight { 0 };
    std::atomic<int> boundPort { 0 };
    std::atomic<int> activeWebSockets { 0 };
    std::atomic<std::uint64_t> meterFramesSent { 0 };

    mutable std::mutex configMutex;
    MeterProvider meterProvider;
    AudioStateProvider audioProvider;
    AudioApplyHandler audioApply;
    AudioMeasureHandler audioMeasure;
    PluginInventoryProvider pluginProvider;
    PluginRescanHandler pluginRescan;
    MidiStateProvider midiProvider;
    MidiInputHandler midiEnableInput;
    MidiPadTestHandler midiPadTest;
    std::string authToken;
    const int meterHz;
    StateBroadcast broadcast;

    mutable std::mutex frameMutex;
    std::string latestFrameJson;
    std::uint64_t latestFrameVersion = 0;

    // LAN mode / pairing
    PairingManager pairing;
    Discovery discovery;
    mutable std::mutex lifecycleMutex;
    std::string listenerAddress;
    int nextPort = 0;

    std::mutex controlMutex;
    std::condition_variable controlCv;
    bool controlStop = false;
    bool rebindRequested = false;
};

Server::Server (StateStore& state)
    : Server (state, Options {})
{
}

Server::Server (StateStore& state, Options options)
    : impl (std::make_unique<Impl> (state, std::move (options)))
{
}

Server::~Server()
{
    stop();
}

bool Server::start()
{
    auto& self = *impl;
    const std::lock_guard<std::mutex> life (self.lifecycleMutex);

    if (self.running.load (std::memory_order_acquire))
        return true;

    if (! self.bindListenerLocked())
        return false;

    self.running.store (true, std::memory_order_release);
    self.broadcast.start();

    {
        const std::lock_guard<std::mutex> lock (self.controlMutex);
        self.controlStop = false;
        self.rebindRequested = false;
    }

    self.meterThread = std::thread ([&self] { self.meterLoop(); });
    self.controlThread = std::thread ([&self] { self.controlLoop(); });

    return true;
}

void Server::stop()
{
    auto& self = *impl;

    {
        const std::lock_guard<std::mutex> lock (self.controlMutex);
        self.controlStop = true;
        self.rebindRequested = false;
    }

    self.controlCv.notify_all();

    // Ends the `/events` loops; each closes its socket within one poll
    // interval, before the listener is torn down.
    self.broadcast.stop();

    if (self.controlThread.joinable())
        self.controlThread.join();

    {
        const std::lock_guard<std::mutex> life (self.lifecycleMutex);
        self.running.store (false, std::memory_order_release);
        self.stopListenerLocked();
    }

    // WebSocket handlers leave their read loop within one short timeout.
    for (int spin = 0; spin < 200
                       && self.activeWebSockets.load (std::memory_order_acquire) > 0;
         ++spin)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }

    if (self.meterThread.joinable())
        self.meterThread.join();

    self.boundPort.store (0, std::memory_order_release);
}

bool Server::isRunning() const noexcept
{
    return impl->running.load (std::memory_order_acquire);
}

bool Server::isListening() const noexcept
{
    return impl->listening.load (std::memory_order_acquire);
}

int Server::getPort() const noexcept
{
    return impl->boundPort.load (std::memory_order_acquire);
}

std::string Server::getAddress() const
{
    const std::lock_guard<std::mutex> life (impl->lifecycleMutex);
    return impl->listenerAddress.empty() ? impl->options.address : impl->listenerAddress;
}

void Server::setMeterProvider (MeterProvider provider)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->meterProvider = std::move (provider);
}

void Server::setAudioHandlers (AudioStateProvider provider,
                               AudioApplyHandler apply,
                               AudioMeasureHandler measure)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->audioProvider = std::move (provider);
    impl->audioApply = std::move (apply);
    impl->audioMeasure = std::move (measure);
}

void Server::setPluginHandlers (PluginInventoryProvider provider,
                                PluginRescanHandler rescan)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->pluginProvider = std::move (provider);
    impl->pluginRescan = std::move (rescan);
}

void Server::setMidiHandlers (MidiStateProvider provider,
                              MidiInputHandler enableInput,
                              MidiPadTestHandler padTest)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->midiProvider = std::move (provider);
    impl->midiEnableInput = std::move (enableInput);
    impl->midiPadTest = std::move (padTest);
}

void Server::setAuthToken (std::string token)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->authToken = std::move (token);
}

bool Server::hasAuthToken() const
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    return ! impl->authToken.empty();
}

PairingManager& Server::getPairing()
{
    return impl->pairing;
}

const PairingManager& Server::getPairing() const
{
    return impl->pairing;
}

Discovery& Server::getDiscovery()
{
    return impl->discovery;
}

const Discovery& Server::getDiscovery() const
{
    return impl->discovery;
}

StateBroadcast& Server::getStateBroadcast()
{
    return impl->broadcast;
}

const StateBroadcast& Server::getStateBroadcast() const
{
    return impl->broadcast;
}

PairingSnapshot Server::setLanMode (bool enabled)
{
    return impl->applyLanMode (enabled);
}

bool Server::isLanModeEnabled() const
{
    return impl->pairing.isLanEnabled();
}

std::uint64_t Server::getMeterFrameCount() const noexcept
{
    return impl->meterFramesSent.load (std::memory_order_acquire);
}

} // namespace Api
