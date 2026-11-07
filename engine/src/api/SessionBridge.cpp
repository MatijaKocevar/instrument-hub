#include "SessionBridge.h"

#include "Session.h"
#include "TrackBridge.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace Api
{

namespace
{
TransportState toTransportState (const AudioEngine::Transport::Snapshot& snapshot) noexcept
{
    TransportState result;
    result.playing = snapshot.playing;
    result.bpm = snapshot.bpm;
    result.ppq = snapshot.ppq;
    result.positionSamples = snapshot.positionSamples;
    return result;
}

double gainToDb (float gain) noexcept
{
    if (gain <= 0.0f)
        return -60.0;

    return juce::jlimit (-60.0, 12.0, 20.0 * std::log10 (static_cast<double> (gain)));
}

bool toSessionKind (const std::string& text, ihub::SessionSlotKind& kind)
{
    if (text == "empty")
    {
        kind = ihub::SessionSlotKind::empty;
        return true;
    }

    if (text == "init-tone")
    {
        kind = ihub::SessionSlotKind::initTone;
        return true;
    }

    if (text == "sampler")
    {
        kind = ihub::SessionSlotKind::sampler;
        return true;
    }

    if (text == "plugin")
    {
        kind = ihub::SessionSlotKind::plugin;
        return true;
    }

    if (text == "audio-input")
    {
        kind = ihub::SessionSlotKind::audioInput;
        return true;
    }

    return false;
}

PatchInstrumentState toPatchInstrument (const ihub::SessionSlotConfig& slotConfig)
{
    PatchInstrumentState instrument;
    instrument.id = slotConfig.patch;
    instrument.name = slotConfig.name;
    instrument.kind = slotConfig.instrumentKind;
    instrument.pluginFormat = slotConfig.pluginFormat;
    instrument.pluginId = slotConfig.pluginId;
    instrument.pluginName = slotConfig.pluginName;
    instrument.pluginManufacturer = slotConfig.pluginManufacturer;
    instrument.pluginVersion = slotConfig.pluginVersion;
    instrument.samplePath = slotConfig.samplePath;
    return instrument;
}

FxChainState toFxChainState (const AudioEngine::FxChainSnapshot& snapshot)
{
    FxChainState chain;
    chain.slot = snapshot.slot;
    chain.instrumentLatencySamples = snapshot.instrumentLatencySamples;
    chain.fxLatencySamples = snapshot.fxLatencySamples;
    chain.latencySamples = snapshot.latencySamples;
    chain.latencyMs = snapshot.latencyMs;
    chain.inserts.reserve (snapshot.inserts.size());

    for (const auto& insert : snapshot.inserts)
    {
        FxInsertState state;
        state.index = insert.index;
        state.type = AudioEngine::toString (insert.type);
        state.kind = insert.kind;
        state.id = insert.id;
        state.name = insert.name;
        state.bypassed = insert.bypassed;
        state.wet = static_cast<double> (insert.wet);
        state.latencySamples = insert.latencySamples;

        for (const auto& [key, value] : insert.parameters)
            state.parameters[key] = static_cast<double> (value);

        chain.inserts.push_back (std::move (state));
    }

    return chain;
}

bool toFxInsertConfig (const FxInsertRequest& request,
                       AudioEngine::FxInsertConfig& config,
                       std::string& error)
{
    AudioEngine::FxInsertType type;

    if (! AudioEngine::fxInsertTypeFromString (request.type, type))
    {
        error = "unknown fx type: \"" + request.type + "\"";
        return false;
    }

    config = {};
    config.type = type;
    config.path = request.path.empty() ? request.id : request.path;
    config.id = request.id;
    config.name = request.name;
    config.bypassed = request.bypassed;
    config.wet = static_cast<float> (juce::jlimit (0.0, 1.0, request.wet));

    for (const auto& [key, value] : request.parameters)
        config.parameters[key] = static_cast<float> (value);

    return true;
}
}

void attachSession (StateStore& state, Server& server, ihub::Session& session)
{
    state.setNumSlots (session.getNumSlots());
    state.setLayout (ihub::toString (session.getLayout()));

    const auto& slotConfigs = session.getSlotConfigs();
    const double sampleRate = session.getEngine().getSampleRate();

    std::vector<FamilyState> families;

    for (int index = 0; index < state.getNumSlots(); ++index)
    {
        if (index < static_cast<int> (slotConfigs.size()))
        {
            const auto& slotConfig = slotConfigs[static_cast<std::size_t> (index)];

            state.setSlotName (index, slotConfig.name);
            state.setSlotIdentity (index,
                                   slotConfig.family,
                                   slotConfig.familyName,
                                   slotConfig.patch,
                                   slotConfig.patchName);
            state.setPatchInstrument (index, toPatchInstrument (slotConfig));
            state.setPatchMix (index,
                               gainToDb (slotConfig.gain),
                               static_cast<double> (slotConfig.pan),
                               slotConfig.muted,
                               slotConfig.solo,
                               slotConfig.colorIndex);

            if (! slotConfig.family.empty())
            {
                auto family = std::find_if (families.begin(), families.end(),
                                            [&slotConfig] (const FamilyState& candidate)
                                            {
                                                return candidate.id == slotConfig.family;
                                            });

                if (family == families.end())
                {
                    FamilyState entry;
                    entry.id = slotConfig.family;
                    entry.name = slotConfig.familyName.empty() ? slotConfig.family
                                                               : slotConfig.familyName;
                    families.push_back (std::move (entry));
                    family = std::prev (families.end());
                }

                PatchState patch;
                patch.slot = index;
                patch.id = slotConfig.patch;
                patch.name = slotConfig.patchName.empty() ? slotConfig.name
                                                          : slotConfig.patchName;
                patch.family = slotConfig.family;
                patch.familyName = slotConfig.familyName;
                family->patches.push_back (std::move (patch));
            }
        }

        state.setSlotLatency (index,
                              session.getSlotLatencySamples (index),
                              sampleRate);
    }

    state.setFamilyTree (std::move (families));

    state.setPatchHandler ([&session] (int slot, const PatchState&, float gain)
    {
        session.setSlotGain (slot, gain);
    });

    state.setPatchSourceHandler ([&session] (int slot,
                                             const PatchSourceRequest& request,
                                             std::string& error)
    {
        const auto& configs = session.getSlotConfigs();

        if (slot < 0 || slot >= static_cast<int> (configs.size()))
        {
            error = "slot is out of range";
            return false;
        }

        ihub::SessionSlotKind kind;

        if (! toSessionKind (request.kind, kind))
        {
            error = "unknown source kind: " + request.kind;
            return false;
        }

        ihub::SessionSlotConfig source = configs[static_cast<std::size_t> (slot)];
        source.kind = kind;
        source.path = juce::String (request.path);
        source.frequency = request.frequency >= 20.0 ? request.frequency
                                                     : source.frequency;
        source.inputChannel = request.inputChannel;
        source.instrumentKind = request.kind;
        source.pluginFormat = request.pluginFormat.empty() ? "VST3"
                                                          : request.pluginFormat;
        source.pluginId = request.pluginId;
        source.pluginName = request.pluginName.empty() ? request.name
                                                       : request.pluginName;
        source.pluginManufacturer = request.pluginManufacturer;
        source.pluginVersion = request.pluginVersion;
        source.samplePath = request.samplePath.empty() && request.kind == "sampler"
                                ? request.path
                                : request.samplePath;

        return session.setSlotSource (slot, source, error);
    });

    state.setPatchEditorHandlers (
        [&session] (int slot, std::string& error)
        {
            return session.openEditor (slot, error);
        },
        [&session] (int slot, std::string& error)
        {
            return session.closeEditor (slot, error);
        });

    state.setPatchReloadHandler ([&session] (int slot, std::string& error)
    {
        return session.reloadPlugin (slot, error);
    });

    state.setPatchStateCaptureHandler (
        [&session] (int slot, PatchStateCapture& capture, std::string& error)
        {
            if (! session.capturePluginState (slot))
            {
                error = "the patch has no resident plugin sound to save";
                return false;
            }

            capture.slot = slot;
            capture.captured = true;
            capture.sizeBytes = session.getPluginStateSize (slot);
            return true;
        });

    state.setFxHandlers (
        [&session] (int slot, FxChainState& chain, std::string& error)
        {
            AudioEngine::FxChainSnapshot snapshot;

            if (! session.getSlotFxChain (slot, snapshot, error))
                return false;

            chain = toFxChainState (snapshot);
            return true;
        },
        [&session, &state] (int slot,
                            const FxCommand& command,
                            FxChainState& chain,
                            std::string& error)
        {
            AudioEngine::FxChainSnapshot snapshot;
            bool applied = false;

            switch (command.kind)
            {
                case FxCommandKind::add:
                {
                    if (command.inserts.empty())
                    {
                        error = "no insert was supplied";
                        return false;
                    }

                    AudioEngine::FxInsertConfig config;

                    if (! toFxInsertConfig (command.inserts.front(), config, error))
                        return false;

                    applied = session.addSlotFx (slot, config,
                                                 command.inserts.front().position,
                                                 snapshot, error);
                    break;
                }

                case FxCommandKind::replace:
                {
                    std::vector<AudioEngine::FxInsertConfig> configs;
                    configs.reserve (command.inserts.size());

                    for (const auto& request : command.inserts)
                    {
                        AudioEngine::FxInsertConfig config;

                        if (! toFxInsertConfig (request, config, error))
                            return false;

                        configs.push_back (std::move (config));
                    }

                    applied = session.replaceSlotFx (slot, configs, snapshot, error);
                    break;
                }

                case FxCommandKind::update:
                {
                    AudioEngine::FxInsertPatch patch;
                    patch.bypassed = command.update.bypassed;

                    if (command.update.wet.has_value())
                        patch.wet = static_cast<float> (*command.update.wet);

                    patch.position = command.update.position;

                    if (command.update.parameters.has_value())
                    {
                        std::map<std::string, float> parameters;

                        for (const auto& [key, value] : *command.update.parameters)
                            parameters[key] = static_cast<float> (value);

                        patch.parameters = std::move (parameters);
                    }

                    applied = session.updateSlotFx (slot, command.index, patch,
                                                    snapshot, error);
                    break;
                }

                case FxCommandKind::remove:
                    applied = session.removeSlotFx (slot, command.index,
                                                    snapshot, error);
                    break;

                case FxCommandKind::reorder:
                    applied = session.reorderSlotFx (slot, command.order,
                                                     snapshot, error);
                    break;

                case FxCommandKind::clear:
                    applied = session.clearSlotFx (slot, snapshot, error);
                    break;
            }

            if (! applied)
                return false;

            state.setSlotLatency (slot,
                                  snapshot.latencySamples,
                                  session.getEngine().getSampleRate());
            chain = toFxChainState (snapshot);
            return true;
        });

    attachTrackState (state, session);

    state.setArmHandler ([&session] (int slot)
    {
        session.armSlot (slot);
    });

    state.setTransportHandler ([&session] (const TransportState& next,
                                           const juce::var& patch)
    {
        auto& transport = session.getTransport();

        if (! patch.isObject())
            return;

        const auto* object = patch.getDynamicObject();

        if (object == nullptr)
            return;

        if (object->hasProperty ("playing"))
            transport.setPlaying (next.playing);

        if (object->hasProperty ("bpm"))
            transport.setBpm (next.bpm);

        if (object->hasProperty ("ppq"))
            transport.setPpq (next.ppq);

        if (object->hasProperty ("positionSamples"))
            transport.setPositionSamples (next.positionSamples);

        session.updateFxTransport();
    });

    state.setSlotRuntimeProvider ([&session] (int slot)
    {
        SlotRuntimeState runtime;
        runtime.armed = session.getArmedSlot() == slot;
        runtime.gain = session.getSwitcher().getSlotGain (slot);
        runtime.state = AudioEngine::SlotSwitcher::toString (
            session.getSwitcher().getSlotState (slot));
        runtime.warmState = session.getWarmStateName (slot);
        runtime.warm = session.isSlotWarm (slot);
        runtime.editorOpen = session.isEditorOpen (slot);
        runtime.error = session.getSlotError (slot).toStdString();
        return runtime;
    });

    session.setSlotStateObserver ([&state, lifetime = state.getLifetime()] (int slot)
    {
        const std::lock_guard<std::mutex> lock (lifetime->mutex);

        if (! lifetime->alive)
            return;

        state.notifyChanged ("warm", slot);
    });

    session.setPageStatusObserver ([&state, lifetime = state.getLifetime()]
    {
        const std::lock_guard<std::mutex> lock (lifetime->mutex);

        if (! lifetime->alive)
            return;

        state.notifyChanged ("page", -1);
    });

    state.setPluginPageProvider ([&session]
    {
        return session.getPluginPageStatuses();
    });

    state.setPageRestartHandler ([&session] (const std::string& page, std::string& error)
    {
        return session.restartPluginPage (page, error);
    });

    state.setDetachHandler ([&session]
    {
        session.setSlotStateObserver ({});
        session.setPageStatusObserver ({});
    });

    state.setTransportProvider ([&session]
    {
        return toTransportState (session.getTransport().snapshot());
    });

    server.setMeterProvider ([&session, &state]
    {
        MeterFrame frame;
        frame.timestampMs = state.getUptimeMs();
        frame.masterPeak = session.getMasterMeterPeak();

        const int slots = session.getNumSlots();
        const int armed = session.getArmedSlot();

        frame.slots.reserve (static_cast<std::size_t> (juce::jmax (0, slots)));

        for (int index = 0; index < slots; ++index)
        {
            SlotMeter meter;
            meter.slot = index;
            meter.peak = session.getSlotMeterPeak (index);
            meter.gain = session.getSwitcher().getSlotGain (index);
            meter.armed = index == armed;
            frame.slots.push_back (meter);
        }

        return frame;
    });

    const int armed = session.getArmedSlot();

    if (armed >= 0)
        (void) state.setArmedSlot (armed);
}

}
