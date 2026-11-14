#include "ProjectCapture.h"

#include "PatchColours.h"
#include "Session.h"
#include "TrackLayoutSave.h"
#include "api/StateStore.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ihub::app
{

    namespace
    {

        project::SourceKind toSourceKind (SessionSlotKind kind) noexcept
        {
            switch (kind)
            {
                case SessionSlotKind::empty: return project::SourceKind::empty;
                case SessionSlotKind::initTone: return project::SourceKind::initTone;
                case SessionSlotKind::sampler: return project::SourceKind::sampler;
                case SessionSlotKind::plugin: return project::SourceKind::plugin;
                case SessionSlotKind::audioInput: return project::SourceKind::audioInput;
            }

            return project::SourceKind::empty;
        }

        bool wasActive (const project::SetFile& base, int slot) noexcept
        {
            return std::any_of (base.slots.begin (),
                                base.slots.end (),
                                [slot] (const project::SlotConfig& candidate)
                                { return candidate.index == slot && candidate.active; });
        }

        project::FxConfig toFxConfig (const AudioEngine::FxInsertSnapshot& insert)
        {
            project::FxConfig fx;
            fx.kind = insert.type == AudioEngine::FxInsertType::vst3 ? project::FxKind::plugin
                                                                     : project::FxKind::builtin;
            fx.type = AudioEngine::toString (insert.type);
            fx.id = insert.id;
            fx.name = insert.name;
            fx.bypassed = insert.bypassed;
            fx.parameters = insert.parameters;

            if (insert.wet < 1.0f)
                fx.parameters["wet"] = insert.wet;

            return fx;
        }

        std::string pluginStateIdForSlot (int slot)
        {
            return "slot-" + std::to_string (slot);
        }

    }

    bool captureProject (ihub::Session& session,
                         const Api::StateStore& state,
                         const project::SetFile& base,
                         const std::vector<int>& touchedSlots,
                         project::SetFile& destination,
                         juce::String& error)
    {
        for (const int slot : touchedSlots)
        {
            if (slot >= 0)
                (void)session.capturePluginState (slot);
        }

        project::SetFile set;
        set.name = base.name.empty () ? "Untitled Set" : base.name;
        set.notes = base.notes;
        set.mappingYaml = base.mappingYaml;

        if (state.getLayout () == "tracks" &&
            !saveTrackLayoutToMapping (
                base.mappingYaml, state.getTracks (), set.mappingYaml, error))
            return false;

        const auto sources = session.copySourceConfigs ();
        const int numSlots = juce::jmin (static_cast<int> (sources.size ()), state.getNumSlots ());

        set.slots.reserve (static_cast<std::size_t> (juce::jmax (0, numSlots)));

        for (int index = 0; index < numSlots; ++index)
        {
            const auto source = sources[static_cast<std::size_t> (index)];
            const auto patch = state.getPatch (index);

            project::SlotConfig slot;
            slot.index = index;
            slot.name = patch.instrument.name.empty () ? patch.name : patch.instrument.name;
            slot.setVolumeDb (static_cast<float> (patch.volumeDb));
            slot.pan = static_cast<float> (patch.pan);
            slot.muted = patch.mute;
            slot.solo = patch.solo;
            slot.colorIndex = patch.colorIndex;
            slot.colour = colourForIndex (patch.colorIndex);
            slot.active = wasActive (base, index);
            slot.instrument.gain = slot.gain;

            slot.instrument.kind = toSourceKind (source.kind);
            slot.instrument.id = patch.instrument.id.empty () ? patch.id : patch.instrument.id;
            slot.instrument.name = slot.name;
            slot.instrument.pluginFormat =
                patch.instrument.pluginFormat.empty ()
                    ? (source.pluginFormat.empty () ? "VST3" : source.pluginFormat)
                    : patch.instrument.pluginFormat;
            slot.instrument.pluginId =
                patch.instrument.pluginId.empty () ? source.pluginId : patch.instrument.pluginId;
            slot.instrument.pluginName = patch.instrument.pluginName.empty ()
                                             ? source.pluginName
                                             : patch.instrument.pluginName;
            slot.instrument.pluginManufacturer = patch.instrument.pluginManufacturer.empty ()
                                                     ? source.pluginManufacturer
                                                     : patch.instrument.pluginManufacturer;
            slot.instrument.pluginVersion = patch.instrument.pluginVersion.empty ()
                                                ? source.pluginVersion
                                                : patch.instrument.pluginVersion;
            slot.instrument.samplePath = patch.instrument.samplePath.empty ()
                                             ? source.samplePath
                                             : patch.instrument.samplePath;
            slot.instrument.inputChannel = source.inputChannel;

            if (source.kind == SessionSlotKind::plugin)
                slot.instrument.pluginPath = source.path.toStdString ();
            else if (source.kind == SessionSlotKind::sampler && slot.instrument.samplePath.empty ())
                slot.instrument.samplePath = source.path.toStdString ();

            AudioEngine::FxChainSnapshot chain;
            std::string chainError;

            if (!session.getSlotFxChain (index, chain, chainError))
            {
                error = "could not read the FX chain for slot " + juce::String (index) + ": " +
                        juce::String (chainError);
                return false;
            }

            for (const auto& insert : chain.inserts)
                slot.fxChain.push_back (toFxConfig (insert));

            if (source.kind == SessionSlotKind::plugin)
            {
                juce::MemoryBlock data;

                if (session.copyPluginStateData (index, data))
                {
                    project::PluginStateChunk chunk;
                    chunk.uuid = pluginStateIdForSlot (index);
                    chunk.format = slot.instrument.pluginFormat;
                    chunk.pluginId = slot.instrument.pluginId;
                    chunk.name = slot.instrument.pluginName;
                    chunk.manufacturer = slot.instrument.pluginManufacturer;
                    chunk.version = slot.instrument.pluginVersion;
                    chunk.data = std::move (data);

                    slot.instrument.pluginState = chunk.uuid;
                    set.pluginStates.push_back (std::move (chunk));
                }
                else
                {
                    slot.instrument.pluginState.clear ();
                }
            }

            set.slots.push_back (std::move (slot));
        }

        const auto transport = session.getTransport ().snapshot ();
        set.transport.bpm = transport.bpm;
        set.transport.positionPpq = transport.ppq;
        set.transport.playing = transport.playing;

        if (!set.validate (error))
            return false;

        destination = std::move (set);
        error.clear ();
        return true;
    }

}
