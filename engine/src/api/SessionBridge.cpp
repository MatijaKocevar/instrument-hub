#include "SessionBridge.h"

#include "Session.h"

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
} // namespace

void attachSession (StateStore& state, Server& server, ihub::Session& session)
{
    // Mirror the composed set's names and chain latency once. These are static
    // for the life of a set; the live values below are read per request.
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

    // PATCH /patches/{slot} -> the fixed slot's atomic gain. The store has
    // already resolved mute/solo, so this only publishes the effective gain.
    state.setPatchHandler ([&session] (int slot, const PatchState&, float gain)
    {
        session.setSlotGain (slot, gain);
    });

    // POST /arm -> SlotSwitcher::armSlot(): one lock-free atomic publish.
    state.setArmHandler ([&session] (int slot)
    {
        session.armSlot (slot);
    });

    // POST /transport -> the session's sample-accurate transport. Only the
    // fields the request sent are applied, so a position that has advanced
    // since the store's last snapshot is never rewound by a stop command.
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
    });

    // GET /state (and the /events mirror): real slot states and gains.
    state.setSlotRuntimeProvider ([&session] (int slot)
    {
        SlotRuntimeState runtime;
        runtime.armed = session.getArmedSlot() == slot;
        runtime.gain = session.getSwitcher().getSlotGain (slot);
        runtime.state = AudioEngine::SlotSwitcher::toString (
            session.getSwitcher().getSlotState (slot));
        runtime.warmState = session.getWarmStateName (slot);
        runtime.warm = session.isSlotWarm (slot);
        return runtime;
    });

    // GET /transport and /state.transport: the running transport, not the
    // values a client happened to POST last.
    state.setTransportProvider ([&session]
    {
        return toTransportState (session.getTransport().snapshot());
    });

    // /meters: peaks measured from the real slot outputs and the master bus.
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

    // Seed the store with the slot the session is already fading to, so a
    // fresh /state request matches the audio before the first arm request.
    const int armed = session.getArmedSlot();

    if (armed >= 0)
        (void) state.setArmedSlot (armed);
}

} // namespace Api
