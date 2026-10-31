// ihub-patch-state-capture-api-test: acceptance check for T132.
//
// Drives the Instrument page's explicit save action end to end:
//
//   1. a TestPlugin patch is armed and given a preset A;
//   2. POST /patches/0/state/capture writes the patch sidecar immediately,
//      while the patch is still armed (no switch-away is needed);
//   3. a second preset B is saved and its blob differs from A;
//   4. saving again is an idempotent confirmation with the same stored bytes;
//   5. switching away and back restores B, and a fresh session built from the
//      same sidecar directory also restores B;
//   6. a generated patch and an out-of-range slot are rejected without writing.
//
// Everything is offline and deterministic: rendering is driven by the test and
// plugin creation is bounded by the message-pump deadline used by T131.

#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"
#include "plugins/PluginInstrument.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <string>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 3;
constexpr int kPluginSlot = 0;
constexpr int kToneSlot = 1;

constexpr double kFrequencyA = 330.0;
constexpr double kFrequencyB = 660.0;
constexpr float kStateMarkerA = 0.3f;
constexpr float kStateMarkerB = 0.7f;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

juce::var parseJson (const std::string& text)
{
    return juce::JSON::parse (juce::String (text));
}

juce::var property (const juce::var& object, const char* name)
{
    if (! object.isObject() || object.getDynamicObject() == nullptr)
        return {};

    return object.getDynamicObject()->getProperty (name);
}

httplib::Result requestWithDispatch (std::function<httplib::Result()> send)
{
    auto future = std::async (std::launch::async, std::move (send));

    while (future.wait_for (std::chrono::milliseconds (0))
           != std::future_status::ready)
    {
        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
    }

    return future.get();
}

httplib::Result postWithDispatch (httplib::Client& client,
                                 const std::string& path,
                                 const std::string& body)
{
    return requestWithDispatch ([&client, path, body]
    {
        return client.Post (path, body, "application/json");
    });
}

float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

double denormaliseFrequency (float normalised)
{
    return 20.0 + static_cast<double> (normalised) * (20000.0 - 20.0);
}

bool approximately (double value, double expected, double tolerance)
{
    return std::abs (value - expected) <= tolerance;
}

void applyPreset (ihub::Session& session,
                  int slot,
                  double frequency,
                  float stateMarker)
{
    auto* instrument = session.getPluginInstrument (slot);

    check (instrument != nullptr, "slot " + std::to_string (slot) + " has a live plugin");

    if (instrument == nullptr)
        return;

    (void) instrument->setParameterValue ("Sine Level", 1.0f);
    (void) instrument->setParameterValue ("Gain", 0.0f);
    (void) instrument->setParameterValue ("Frequency", normaliseFrequency (frequency));
    (void) instrument->setParameterValue ("State Marker", stateMarker);
}

float stateMarker (ihub::Session& session, int slot)
{
    auto* instrument = session.getPluginInstrument (slot);
    return instrument != nullptr ? instrument->getParameterValue ("State Marker") : -1.0f;
}

double measurePitch (ihub::Session& session)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    // Let the equal-power crossfade finish before measuring.
    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);
    }

    int crossings = 0;
    int frames = 0;
    float previous = 0.0f;

    for (int block = 0; block < 192; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const float sample = buffer.getSample (0, frame);

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

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = 1;
    config.initialSlot = -1;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    config.slots.resize (kSlots);

    auto& plugin = config.slots[static_cast<std::size_t> (kPluginSlot)];
    plugin.kind = ihub::SessionSlotKind::plugin;
    plugin.path = bundlePath;
    plugin.pluginFormat = "VST3";
    plugin.pluginName = "TestPlugin";
    plugin.instrumentKind = "plugin";
    plugin.name = "Saved Sound";
    plugin.patch = "saved-sound";
    plugin.patchName = plugin.name;

    auto& tone = config.slots[static_cast<std::size_t> (kToneSlot)];
    tone.kind = ihub::SessionSlotKind::initTone;
    tone.frequency = 220.0;
    tone.name = "Tone";
    tone.patch = "tone";
    tone.patchName = tone.name;

    return config;
}

juce::MemoryBlock readBytes (const juce::File& file)
{
    juce::MemoryBlock data;
    (void) file.loadFileAsData (data);
    return data;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String bundlePath (IHUB_TESTPLUGIN_VST3);

    if (bundlePath.isEmpty() || ! juce::File (bundlePath).exists())
    {
        std::cerr << "patch-state-capture-api-test: FAILED, TestPlugin.vst3 is missing"
                  << std::endl;
        return 1;
    }

    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("ihub-t132-"
                                             + juce::String::toHexString (
                                                 juce::Random::getSystemRandom().nextInt64()));
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");

    (void) tempRoot.deleteRecursively();
    (void) tempRoot.createDirectory();

    const auto config = makeConfig (bundlePath);
    const auto stateFile = stateDirectory.getChildFile ("slot-0.bin");

    //--------------------------------------------------------------------------
    // First run: an explicit save in place, then switch away and back.

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        juce::String buildError;

        if (! session.build (config, buildError))
        {
            std::cerr << "patch-state-capture-api-test: FAILED to build: "
                      << buildError << std::endl;
            return 1;
        }

        Api::StateStore state (session.getNumSlots());
        Api::Server::Options options;
        options.address = "127.0.0.1";
        options.port = 0;
        options.meterHz = 30;

        Api::Server server (state, options);
        Api::attachSession (state, server, session);

        if (! server.start())
        {
            std::cerr << "patch-state-capture-api-test: FAILED to start the server"
                      << std::endl;
            return 1;
        }

        httplib::Client client ("127.0.0.1", server.getPort());
        client.set_connection_timeout (2, 0);
        client.set_read_timeout (20, 0);

        auto response = postWithDispatch (client, "/arm", "{\"slot\":0}");
        check (response && response->status == 200, "POST /arm 0 reaches the engine");
        check (session.waitForSlotActive (kPluginSlot, 15000),
               "the plugin patch warms and becomes active");

        applyPreset (session, kPluginSlot, kFrequencyA, kStateMarkerA);

        response = postWithDispatch (client,
                                     "/patches/0/state/capture",
                                     "{}");
        check (response && response->status == 200,
               "POST /patches/0/state/capture returns 200");

        auto body = response ? parseJson (response->body) : juce::var {};

        const auto sizeA = static_cast<juce::int64> (property (body, "sizeBytes"));

        check (static_cast<int> (property (body, "slot")) == kPluginSlot,
               "the capture answer names the patch slot");
        check (static_cast<bool> (property (body, "captured")),
               "the capture answer confirms the sound was saved");
        check (sizeA > 0, "the capture answer reports a non-empty state blob");
        check (session.hasPluginState (kPluginSlot)
                   && session.getPluginStateSize (kPluginSlot) == sizeA,
               "the session holds exactly the reported blob");
        check (stateFile.existsAsFile() && stateFile.getSize() > 0,
               "the explicit save wrote the sidecar without switching away");
        check (stateFile.getSize() > sizeA,
               "the sidecar wraps the reported blob with its identity header");

        const auto bytesA = readBytes (stateFile);

        applyPreset (session, kPluginSlot, kFrequencyB, kStateMarkerB);

        response = postWithDispatch (client,
                                     "/patches/0/state/capture",
                                     "{}");
        body = response ? parseJson (response->body) : juce::var {};
        const auto sizeB = static_cast<juce::int64> (property (body, "sizeBytes"));

        check (response && response->status == 200 && sizeB > 0,
               "a second, different preset saves in place");

        const auto bytesB = readBytes (stateFile);

        check (bytesA != bytesB, "the saved state changed with the preset");

        response = postWithDispatch (client,
                                     "/patches/0/state/capture",
                                     "{}");
        body = response ? parseJson (response->body) : juce::var {};

        check (response && response->status == 200
                   && static_cast<bool> (property (body, "captured")),
               "saving the same sound again is an idempotent confirmation");
        check (readBytes (stateFile) == bytesB,
               "an unchanged save leaves the stored blob intact");

        // The generated patch has no plugin instance to capture.
        response = postWithDispatch (client, "/patches/1/state/capture", "{}");
        check (response && response->status == 400,
               "a generated patch rejects an explicit save");
        check (response
                   && response->body.find ("resident plugin") != std::string::npos,
               "the rejection explains that there is no resident plugin");

        response = postWithDispatch (client, "/patches/99/state/capture", "{}");
        check (response && response->status == 404,
               "an out-of-range slot is rejected");

        // Switching away and back must restore the explicitly saved sound.
        response = postWithDispatch (client, "/arm", "{\"slot\":1}");
        check (response && response->status == 200 && session.waitForSlotActive (kToneSlot, 5000),
               "switching to the tone patch succeeds");

        response = postWithDispatch (client, "/arm", "{\"slot\":0}");
        check (response && response->status == 200 && session.waitForSlotActive (kPluginSlot, 15000),
               "switching back arms the plugin patch");

        const auto markerAfterRoundTrip = stateMarker (session, kPluginSlot);
        const auto pitchAfterRoundTrip = measurePitch (session);

        check (approximately (markerAfterRoundTrip, kStateMarkerB, 0.01),
               "switching away and back restores the explicitly saved marker");
        check (approximately (pitchAfterRoundTrip, kFrequencyB, 40.0),
               "switching away and back restores the explicitly saved sound ("
                   + std::to_string (pitchAfterRoundTrip) + " Hz)");

        server.stop();
    }

    //--------------------------------------------------------------------------
    // Restart: a fresh session reads the explicitly saved sidecar.

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        check (session.hasPluginState (kPluginSlot)
                   && session.getPluginStateSize (kPluginSlot) > 0,
               "the explicitly saved blob is loaded at restart");

        juce::String buildError;

        if (! session.build (config, buildError))
        {
            std::cerr << "patch-state-capture-api-test: FAILED to rebuild: "
                      << buildError << std::endl;
            return 1;
        }

        check (session.waitForSlotWarm (kPluginSlot, 15000),
               "the plugin patch warms after the restart");
        session.armSlot (kPluginSlot);
        check (session.waitForSlotActive (kPluginSlot, 5000),
               "the plugin patch arms after the restart");

        auto* instrument = session.getPluginInstrument (kPluginSlot);
        const auto frequency = instrument != nullptr
                                   ? denormaliseFrequency (
                                         instrument->getParameterValue ("Frequency"))
                                   : 0.0;
        const auto marker = stateMarker (session, kPluginSlot);
        const auto pitch = measurePitch (session);

        check (approximately (marker, kStateMarkerB, 0.01),
               "the restart restores the saved state marker ("
                   + std::to_string (marker) + ")");
        check (approximately (frequency, kFrequencyB, 5.0),
               "the restart restores the saved frequency ("
                   + std::to_string (frequency) + " Hz)");
        check (approximately (pitch, kFrequencyB, 40.0),
               "the restart renders the saved sound ("
                   + std::to_string (pitch) + " Hz)");
    }

    (void) tempRoot.deleteRecursively();

    if (failures > 0)
    {
        std::cerr << "patch-state-capture-api-test: " << failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "patch-state-capture-api-test: all checks passed" << std::endl;
    return 0;
}
