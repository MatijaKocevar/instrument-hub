#include "ConcurrentFixture.h"

#include "plugins/SandboxHost.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <thread>

namespace ConcurrentInstances
{

const std::array<double, kSlots> kFrequencies {
    220.0, 277.18, 329.63, 392.0, 440.0, 523.25, 659.25, 783.99
};

const std::array<float, kSlots> kStateMarkers {
    0.11f, 0.22f, 0.33f, 0.44f, 0.55f, 0.66f, 0.77f, 0.88f
};

namespace
{
void pumpMessages()
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating();
        manager != nullptr && manager->isThisTheMessageThread())
    {
        manager->runDispatchLoopUntil (2);
        return;
    }
   #endif

    juce::Thread::sleep (2);
}
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.device.preferLoopback = false;
    config.layout = ihub::SessionLayout::tracks;
    config.neighbourCount = 2;
    config.initialSlot = 0;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (kSlots);
    config.warmSlots.clear();

    for (int slot = 0; slot < kSlots; ++slot)
    {
        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
        slotConfig.kind = ihub::SessionSlotKind::plugin;
        slotConfig.path = bundlePath;
        slotConfig.pluginFormat = "VST3";
        slotConfig.pluginId = "com.instrumenthub.TestPlugin";
        slotConfig.pluginName = "TestPlugin";
        slotConfig.instrumentKind = "plugin";
        slotConfig.name = "Track " + std::to_string (slot + 1);
        slotConfig.family = "track-" + std::to_string (slot + 1);
        slotConfig.familyName = slotConfig.name;
        slotConfig.patch = slotConfig.family;
        slotConfig.patchName = slotConfig.name;
        slotConfig.frequency = kFrequencies[static_cast<std::size_t> (slot)];
        slotConfig.gain = 1.0f;

        ihub::SessionTrackConfig track;
        track.id = slotConfig.family;
        track.name = slotConfig.name;
        track.slot = slot;
        config.tracks.push_back (std::move (track));
        config.warmSlots.push_back (slot);
    }

    return config;
}

juce::File makeWineBundle (const juce::File& source, const juce::File& root)
{
    auto directory = root.getChildFile ("vstbridge");

    if (! directory.createDirectory())
        return {};

    auto link = directory.getChildFile (source.getFileName());
    (void) link.deleteRecursively();

    if (! source.createSymbolicLink (link, true))
        return {};

    return link;
}

bool waitForWarm (ihub::Session& session, int slot, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    for (;;)
    {
        if (session.isSlotWarm (slot))
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

        pumpMessages();
    }
}

bool waitForAllWarm (ihub::Session& session, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    for (;;)
    {
        bool allWarm = true;

        for (int slot = 0; slot < kSlots; ++slot)
            allWarm = session.isSlotWarm (slot) && allWarm;

        if (allWarm)
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

        pumpMessages();
    }
}

float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

double denormaliseFrequency (float normalised)
{
    return 20.0 + static_cast<double> (normalised) * (20000.0 - 20.0);
}

void applyPreset (ihub::Session& session, int slot)
{
    auto* host = session.getSandboxSlotControl (slot);

    if (host == nullptr)
        return;

    const std::vector<ihub::plugins::SandboxParameterEdit> edits {
        { "Sine Level", 1.0f },
        { "Gain", 0.0f },
        { "Frequency",
          normaliseFrequency (kFrequencies[static_cast<std::size_t> (slot)]) },
        { "State Marker", kStateMarkers[static_cast<std::size_t> (slot)] }
    };

    (void) host->setParameters (edits);
}

double measurePitch (ihub::Session& session, int slot)
{
    session.armSlot (slot);

    if (! session.waitForSlotActive (slot, 30000))
        return -1.0;

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        midi.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);
    }

    int crossings = 0;
    int frames = 0;
    float previous = 0.0f;

    for (int block = 0; block < 192; ++block)
    {
        buffer.clear();
        midi.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const auto sample = buffer.getSample (0, frame);

            if (previous <= 0.0f && sample > 0.0f)
                ++crossings;

            previous = sample;
            ++frames;
        }
    }

    return frames > 0 ? static_cast<double> (crossings) * kSampleRate
                            / static_cast<double> (frames)
                      : 0.0;
}

juce::String stateHash (const juce::MemoryBlock& state)
{
    if (state.getSize() == 0)
        return {};

    std::uint64_t hash = 1469598103934665603ULL;
    const auto* bytes = static_cast<const std::uint8_t*> (state.getData());

    for (std::size_t index = 0; index < state.getSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }

    return juce::String::toHexString (static_cast<juce::int64> (hash));
}

juce::String storedStateHash (const ihub::Session& session, int slot)
{
    juce::MemoryBlock state;
    (void) session.copyPluginStateData (slot, state);
    return stateHash (state);
}

} // namespace ConcurrentInstances
