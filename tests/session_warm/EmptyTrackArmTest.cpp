#include "ProtectedWarmFixtures.h"

#include "Session.h"

#include <cmath>
#include <string>

namespace ProtectedWarmTest
{

namespace
{
constexpr int kAssignedTracks = 3;
constexpr int kRenderBlocks = 32;

double renderRms (ihub::Session& session, int blocks)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    double sumSquares = 0.0;
    int frames = 0;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const double sample = buffer.getSample (0, frame);
            sumSquares += sample * sample;
            ++frames;
        }
    }

    return frames > 0 ? std::sqrt (sumSquares / static_cast<double> (frames)) : 0.0;
}

ihub::SessionConfig makeTrackConfig()
{
    ihub::SessionConfig config;
    config.layout = ihub::SessionLayout::tracks;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.initialSlot = 0;
    config.neighbourCount = 2;
    config.loadDelayMilliseconds = 5;
    config.slots.resize (kSlots);

    for (int index = 0; index < kSlots; ++index)
    {
        auto& slot = config.slots[static_cast<std::size_t> (index)];
        slot.name = "track-" + std::to_string (index + 1);
        slot.kind = index < kAssignedTracks ? ihub::SessionSlotKind::initTone
                                            : ihub::SessionSlotKind::empty;
        slot.frequency = 110.0 * static_cast<double> (index + 1);
    }

    return config;
}
}

void runEmptyTrackScenario()
{
    ihub::Session session;
    juce::String error;

    if (! session.build (makeTrackConfig(), error))
    {
        check (false, "the empty-track session builds: " + error.toStdString());
        return;
    }

    bool assignedWarm = true;

    for (int slot = 0; slot < kAssignedTracks; ++slot)
        assignedWarm = session.waitForSlotWarm (slot, 5000) && assignedWarm;

    check (assignedWarm, "tracks 1-3 warm before the empty arm");

    session.armSlot (2);
    check (session.waitForSlotActive (2, 5000),
           "track 3 reaches active before the empty arm");

    const double playingRms = renderRms (session, kRenderBlocks);
    check (playingRms > 0.0, "the assigned track renders before the empty arm");

    session.armSlot (3);

    check (session.getArmedSlot() == 3,
           "arming the empty track keeps it as the requested slot");
    check (session.getWarmState (3) == ihub::SessionWarmState::active,
           "the empty track completes the arm without a warm request");
    check (session.getSwitcher().getArmedSlot() == 3,
           "the switcher fades to the empty track");
    check (session.getWarmState (2) != ihub::SessionWarmState::active,
           "the previous track stops being active immediately");

    (void) renderRms (session, 2);
    const double silentRms = renderRms (session, kRenderBlocks);
    check (silentRms < 1.0e-6,
           "the armed empty track renders silence (" + std::to_string (silentRms) + ")");
    check (session.getSwitcher().getSlotGain (3) > 0.99f,
           "the empty track reaches full crossfade gain");
    check (session.getSwitcher().getSlotGain (2) <= 0.0f,
           "the previous track fades to zero gain");
    check (session.getSwitcher().getSlotState (2)
               == AudioEngine::SlotSwitcher::State::idle,
           "the previous track becomes idle");
}

}
