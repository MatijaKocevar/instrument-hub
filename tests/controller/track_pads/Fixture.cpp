#include "Fixture.h"

#include "app/surface/TrackSurface.h"

#include "controller/LaunchpadProtocol.h"

#include <array>
#include <filesystem>
#include <utility>

#ifndef IHUB_TRACKS_YAML
 #define IHUB_TRACKS_YAML "shared/tracks-set.yaml"
#endif

namespace TrackPadTest
{

namespace
{
constexpr int trackCount = 8;

struct InsertState
{
    std::string id;
    bool bypassed = false;
};
}

bool Fixture::load (std::string& errorText)
{
    state = std::make_unique<Api::StateStore> (trackCount);
    state->setLayout ("tracks");

    std::vector<Api::TrackConfig> tracks;
    tracks.reserve (trackCount);

    for (int slot = 0; slot < trackCount; ++slot)
    {
        Api::TrackConfig track;
        track.id = "track-" + std::to_string (slot + 1);
        track.name = "Track " + std::to_string (slot + 1);
        track.slot = slot;
        track.volumeStep = Api::trackVolumeDefaultStep;

        if (slot == 0)
            track.hotFx = { "eq", "comp" };

        tracks.push_back (std::move (track));
        state->setPatchMix (slot, 0.0, 0.0, false, false, slot);
    }

    state->setTracks (std::move (tracks));

    std::array<std::vector<InsertState>, trackCount> inserts;

    for (std::size_t hot = 0; hot < 2; ++hot)
        inserts[0].push_back ({ hot == 0 ? "eq" : "comp", false });

    auto insertState = std::make_shared<std::array<std::vector<InsertState>, trackCount>> (
        std::move (inserts));

    state->setFxHandlers (
        [insertState] (int slot, Api::FxChainState& chain, std::string&)
        {
            chain = {};
            chain.slot = slot;

            if (slot < 0 || slot >= trackCount)
                return true;

            for (std::size_t index = 0;
                 index < (*insertState)[static_cast<std::size_t> (slot)].size();
                 ++index)
            {
                const auto& source = (*insertState)[static_cast<std::size_t> (slot)][index];

                Api::FxInsertState insert;
                insert.index = static_cast<int> (index);
                insert.id = source.id;
                insert.name = source.id;
                insert.bypassed = source.bypassed;
                chain.inserts.push_back (std::move (insert));
            }

            return true;
        },
        [this, insertState] (int slot, const Api::FxCommand& command,
                             Api::FxChainState& chain, std::string&)
        {
            if (command.kind != Api::FxCommandKind::update
                || command.index < 0
                || command.index >= static_cast<int> (chain.inserts.size()))
                return false;

            auto& insert = chain.inserts[static_cast<std::size_t> (command.index)];

            if (command.update.bypassed.has_value())
            {
                insert.bypassed = *command.update.bypassed;
                toggledInserts.push_back (insert.id);

                if (slot >= 0 && slot < trackCount)
                    (*insertState)[static_cast<std::size_t> (slot)][static_cast<std::size_t> (command.index)].bypassed
                        = *command.update.bypassed;
            }

            return true;
        });

    state->setTrackGainHandler ([this] (int slot, float gain)
    {
        gains.push_back ({ slot, gain });
    });

    driver.setMessageSink ([this] (const juce::MidiMessage& message)
    {
        sent.emplace_back (message.getRawData(),
                           message.getRawData() + message.getRawDataSize());
    });

    Controller::Mapping::Listeners mappingListeners;

    mappingListeners.armSlot = [this] (int slot)
    {
        (void) state->setArmedSlot (slot);
    };

    mappingListeners.padEvent = [this] (int,
                                        const Controller::PadBinding* binding,
                                        bool pressed)
    {
        if (pressed && binding != nullptr)
            ihub::app::applyTrackPadAction (*state, *binding);
    };

    mapping.setListeners (std::move (mappingListeners));

    Controller::LaunchpadDriver::DriverListeners driverListeners;

    driverListeners.padEvent = [this] (int pad, bool pressed)
    {
        mapping.handlePad (pad, pressed);
    };

    driver.setListeners (std::move (driverListeners));

    Controller::MappingError error;

    if (! mapping.load (std::filesystem::path (IHUB_TRACKS_YAML), &error))
    {
        errorText = error.message;
        return false;
    }

    return true;
}

void Fixture::press (int pad)
{
    const int note = Launchpad::padNoteForIndex (pad);
    driver.injectMidiMessage (juce::MidiMessage::noteOn (
        1, note, static_cast<juce::uint8> (127)));
}

void Fixture::release (int pad)
{
    const int note = Launchpad::padNoteForIndex (pad);
    driver.injectMidiMessage (juce::MidiMessage::noteOn (
        1, note, static_cast<juce::uint8> (0)));
}

Controller::TrackLedSnapshot Fixture::leds() const
{
    return ihub::app::trackLedSnapshotFor (state->snapshot(), *state);
}

void Fixture::paint()
{
    sent.clear();

    const auto mappingCopy = mapping.getMapping();

    if (mappingCopy == nullptr)
        return;

    ihub::app::paintTrackLeds (driver, *mappingCopy, leds());
}

int Fixture::redPads() const
{
    int count = 0;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
        if (driver.getPadLed (pad).velocity
            == Launchpad::velocityFor (Launchpad::Colour::Red))
            ++count;

    return count;
}

}
