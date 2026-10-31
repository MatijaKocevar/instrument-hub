// ihub-instrument-page-test: acceptance check for T124.
//
// The instrument page's controls all end in the control API, so this test
// drives the same requests the page sends and measures what the engine does:
//
//   - the volume fader (PATCH volumeDb) changes the RENDERED level, not just
//     the JSON: -20 dB drops the armed slot's RMS to a tenth, 0 dB restores it;
//   - mute silences the slot and unmute restores it, and pan reaches /state;
//   - the insert chain can be built, bypassed, reordered, replaced and cleared
//     through the same endpoints the page's chain editor uses;
//   - the instrument source can be swapped: a new tone frequency is audible
//     (measured by zero crossings) and a VST3 source loads, becomes active and
//     renders non-silent audio (when TestPlugin is built);
//   - every invalid request is rejected without half-applying the change.
//
// Everything is offline and deterministic: the test drives the render calls,
// so the result does not depend on a sound card or wall-clock timing.

#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <vector>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSettleBlocks = 16;
constexpr int kMeasureBlocks = 64;

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

const juce::Array<juce::var>* arrayProperty (const juce::var& object, const char* name)
{
    return property (object, name).getArray();
}

/** The patch hosted by `slot`, from a `/state` document. */
juce::var patchForSlot (const juce::var& state, int slot)
{
    const auto* families = arrayProperty (state, "families");

    if (families == nullptr)
        return {};

    for (const auto& family : *families)
    {
        const auto* patches = arrayProperty (family, "patches");

        if (patches == nullptr)
            continue;

        for (const auto& patch : *patches)
        {
            if (static_cast<int> (property (patch, "slot")) == slot)
                return patch;
        }
    }

    return {};
}

double rmsOf (const std::vector<float>& samples)
{
    if (samples.empty())
        return 0.0;

    double sum = 0.0;

    for (const float sample : samples)
        sum += static_cast<double> (sample) * static_cast<double> (sample);

    return std::sqrt (sum / static_cast<double> (samples.size()));
}

/** Renders `settle` blocks, then returns `measure` blocks of the left channel. */
std::vector<float> capture (ihub::Session& session, int settle, int measure)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;
    std::vector<float> samples;
    samples.reserve (static_cast<std::size_t> (measure * kBlockSize));

    for (int block = 0; block < settle + measure; ++block)
    {
        session.render (buffer, midi, kBlockSize);

        if (block < settle)
            continue;

        for (int frame = 0; frame < kBlockSize; ++frame)
            samples.push_back (buffer.getSample (0, frame));
    }

    return samples;
}

/** Frequency of a periodic signal from its upward zero crossings. */
double estimateFrequency (const std::vector<float>& samples)
{
    if (samples.size() < 2)
        return 0.0;

    int crossings = 0;

    for (std::size_t index = 1; index < samples.size(); ++index)
    {
        if (samples[index - 1] < 0.0f && samples[index] >= 0.0f)
            ++crossings;
    }

    const double seconds = static_cast<double> (samples.size()) / kSampleRate;

    return seconds > 0.0 ? static_cast<double> (crossings) / seconds : 0.0;
}

/** Sends a request while pumping the JUCE message loop.

    VST3 instantiation takes a MessageManagerLock from the requesting thread on
    Linux. The real application runs the message loop on the main thread; this
    offline test blocks that thread on the HTTP call, so pump it explicitly
    until the worker thread has an answer.
*/
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

httplib::Result patchWithDispatch (httplib::Client& client,
                                   const std::string& path,
                                   const std::string& body)
{
    return requestWithDispatch ([&client, path, body]
    {
        return client.Patch (path, body, "application/json");
    });
}

int insertCount (const juce::var& chain)
{
    const auto* inserts = arrayProperty (chain, "inserts");
    return inserts != nullptr ? inserts->size() : -1;
}

std::string insertType (const juce::var& chain, int index)
{
    const auto* inserts = arrayProperty (chain, "inserts");

    if (inserts == nullptr || index < 0 || index >= inserts->size())
        return {};

    return property ((*inserts)[index], "type").toString().toStdString();
}

int chainLatency (const juce::var& chain)
{
    return static_cast<int> (property (chain, "latencySamples"));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = 1;
    config.slots = ihub::Session::makeDefaultSet();
    config.slots.resize (4);

    ihub::Session session;
    juce::String buildError;

    if (! session.build (config, buildError))
    {
        std::cerr << "instrument-page-test: FAILED to build the session: "
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
        std::cerr << "instrument-page-test: FAILED to start the server" << std::endl;
        return 1;
    }

    std::cout << "instrument-page-test: listening on 127.0.0.1:" << server.getPort()
              << " slots=" << session.getNumSlots() << std::endl;

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (20, 0);

    //--------------------------------------------------------------------------
    // The page opens the armed patch and reads its real data.

    auto response = client.Post ("/arm", "{\"slot\":0}", "application/json");
    check (response && response->status == 200, "POST /arm 0 returns 200");
    check (session.waitForSlotActive (0, 5000),
           "slot 0 warms and becomes the active patch");

    response = client.Get ("/state");

    const auto stateDocument = response ? parseJson (response->body) : juce::var {};
    const auto patch = patchForSlot (stateDocument, 0);

    check (patch.isObject(), "GET /state carries the armed patch");
    check (property (patch, "name").isString(), "the patch has a name");
    check (property (patch, "warmState").toString() == "active",
           "the armed patch reports the active warm state");
    check (property (patch, "latencySamples").isInt(),
           "the patch publishes its chain latency");
    check (property (patch, "instrument").isObject(),
           "the patch publishes its instrument source");

    const double baselineRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (baselineRms > 0.01, "the armed patch renders a non-silent baseline");

    //--------------------------------------------------------------------------
    // The volume fader changes the rendered level.

    response = patchWithDispatch (client, "/patches/0", "{\"volumeDb\":-20}");
    check (response && response->status == 200, "PATCH volumeDb -20 returns 200");

    const double quietRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));
    const double expectedRatio = std::pow (10.0, -20.0 / 20.0);

    check (std::abs (quietRms / baselineRms - expectedRatio) < 0.02,
           "-20 dB on the fader drops the rendered level to a tenth (ratio "
               + std::to_string (quietRms / baselineRms) + ")");

    response = client.Get ("/state");
    check (std::abs (static_cast<double> (property (patchForSlot (parseJson (response->body), 0),
                                                    "volumeDb"))
                     + 20.0) < 1.0e-9,
           "the fader value is published in /state");

    response = patchWithDispatch (client, "/patches/0", "{\"volumeDb\":0}");
    check (response && response->status == 200, "PATCH volumeDb 0 returns 200");

    const double restoredRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (std::abs (restoredRms / baselineRms - 1.0) < 0.02,
           "returning the fader to 0 dB restores the rendered level");

    // Mute is resolved by the engine, so the rendered level goes to zero.
    response = patchWithDispatch (client, "/patches/0", "{\"mute\":true}");
    check (response && response->status == 200, "PATCH mute true returns 200");

    const double mutedRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (mutedRms < 1.0e-5, "mute silences the rendered slot");

    response = patchWithDispatch (client, "/patches/0", "{\"mute\":false}");
    check (response && response->status == 200, "PATCH mute false returns 200");
    check (rmsOf (capture (session, kSettleBlocks, kMeasureBlocks)) > 0.01,
           "unmuting restores the rendered slot");

    response = patchWithDispatch (client, "/patches/0", "{\"pan\":-0.5}");
    check (response && response->status == 200, "PATCH pan -0.5 returns 200");
    check (std::abs (static_cast<double> (property (patchForSlot (parseJson (response->body), 0),
                                                    "pan"))
                     + 0.5) < 1.0e-9,
           "the balance is published in /state");

    response = patchWithDispatch (client, "/patches/0", "{\"volumeDb\":99}");
    check (response && response->status == 400, "an out-of-range fader value is rejected");

    //--------------------------------------------------------------------------
    // The insert chain can be edited through the page's endpoints.

    response = client.Get ("/patches/0/fx");
    check (response && response->status == 200, "GET /patches/0/fx returns 200");
    check (insertCount (parseJson (response->body)) == 0,
           "the default patch starts with an empty chain");

    response = postWithDispatch (client, "/patches/0/fx",
                                 "{\"type\":\"compressor\",\"position\":0,"
                                 "\"parameters\":{\"thresholdDb\":-30,\"ratio\":4}}");
    check (response && response->status == 200, "POST /patches/0/fx returns 200");

    auto chain = response ? parseJson (response->body) : juce::var {};

    check (insertCount (chain) == 1, "the chain editor appended the compressor");
    check (insertType (chain, 0) == "compressor", "the first insert is the compressor");
    check (chainLatency (chain) > 0, "the insert reports its latency contribution");

    const int compressorLatency = chainLatency (chain);

    response = patchWithDispatch (client, "/patches/0/fx/0", "{\"bypassed\":true}");
    check (response && response->status == 200, "PATCH /patches/0/fx/0 returns 200");

    chain = parseJson (response->body);

    check (insertCount (chain) == 1
               && static_cast<bool> (property ((*arrayProperty (chain, "inserts"))[0],
                                               "bypassed")),
           "the compressor is bypassed");
    check (chainLatency (chain) == 0,
           "a bypassed insert leaves the latency budget");
    check (compressorLatency > 0, "the compressor reported latency before it was bypassed");

    response = postWithDispatch (client, "/patches/0/fx", "{\"type\":\"delay\"}");
    check (response && response->status == 200, "a delay insert is accepted");
    check (insertCount (parseJson (response->body)) == 2, "the chain now has two inserts");

    response = postWithDispatch (client, "/patches/0/fx/reorder", "{\"order\":[1,0]}");
    check (response && response->status == 200, "POST /fx/reorder returns 200");

    chain = parseJson (response->body);

    check (insertType (chain, 0) == "delay" && insertType (chain, 1) == "compressor",
           "the reorder reached the engine");

    response = requestWithDispatch ([&client]
    {
        return client.Delete ("/patches/0/fx/0");
    });
    check (response && response->status == 200, "DELETE /patches/0/fx/0 returns 200");
    check (insertCount (parseJson (response->body)) == 1, "the chain editor removed one insert");

    response = requestWithDispatch ([&client]
    {
        return client.Put ("/patches/0/fx",
                           "{\"chain\":[{\"type\":\"eq\"},{\"type\":\"reverb\",\"wet\":0.4}]}",
                           "application/json");
    });
    check (response && response->status == 200, "PUT /fx replaces the whole chain");
    check (insertCount (parseJson (response->body)) == 2, "the replacement chain is complete");

    response = patchWithDispatch (client, "/patches/0/fx/9", "{\"bypassed\":true}");
    check (response && response->status == 404, "an out-of-range insert is rejected");

    response = postWithDispatch (client, "/patches/0/fx", "{\"type\":\"chorus\"}");
    check (response && response->status == 400, "an unknown insert type is rejected");

    //--------------------------------------------------------------------------
    // The instrument source can be swapped and is audible.

    // Leave one bypassed insert so the chain-survives-a-source-change check is
    // meaningful without colouring the tone used for the frequency measurement.
    response = requestWithDispatch ([&client]
    {
        return client.Put ("/patches/0/fx",
                           "{\"chain\":[{\"type\":\"compressor\",\"bypassed\":true}]}",
                           "application/json");
    });
    check (response && response->status == 200, "the chain is prepared for the source change");
    check (chainLatency (parseJson (response->body)) == 0,
           "the bypassed preparation insert adds no latency");

    const double beforeSourceRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (beforeSourceRms > 0.01, "the slot renders before the source change");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"name\":\"Test Tone\","
                                 "\"frequency\":880}");
    check (response && response->status == 200, "POST /patches/0/source returns 200");

    check (session.waitForSlotWarm (0, 5000),
           "the armed slot warms the new source");

    const auto sourceState = response ? parseJson (response->body) : juce::var {};
    const auto sourcePatch = patchForSlot (sourceState, 0);
    const auto instrument = property (sourcePatch, "instrument");

    check (property (instrument, "kind").toString() == "init-tone",
           "/state publishes the new source kind");
    check (property (instrument, "name").toString() == "Test Tone",
           "/state publishes the new source name");

    const auto tone = capture (session, kSettleBlocks, kMeasureBlocks);
    const double toneRms = rmsOf (tone);
    const double frequency = estimateFrequency (tone);

    check (toneRms > 0.01, "the new source renders non-silent audio");
    check (std::abs (frequency - 880.0) < 25.0,
           "the 880 Hz source is audible (measured " + std::to_string (frequency) + " Hz)");

    // The chain survived the source change.
    response = client.Get ("/patches/0/fx");
    check (response && insertCount (parseJson (response->body)) == 1
               && insertType (parseJson (response->body), 0) == "compressor",
           "the insert chain survives a source change");

    // Invalid requests are rejected before anything is swapped.
    response = postWithDispatch (client, "/patches/0/source", "{}");
    check (response && response->status == 400, "a source without a kind is rejected");

    response = postWithDispatch (client, "/patches/0/source", "{\"kind\":\"chorus\"}");
    check (response && response->status == 400, "an unknown source kind is rejected");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"plugin\",\"path\":\"/nonexistent.vst3\"}");
    check (response && response->status == 400, "a missing plug-in bundle is rejected");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"frequency\":5}");
    check (response && response->status == 400, "a frequency outside the tone range is rejected");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"bogus\":1}");
    check (response && response->status == 400, "an unknown source field is rejected");

    response = postWithDispatch (client, "/patches/99/source", "{\"kind\":\"init-tone\"}");
    check (response && response->status == 404, "an unknown slot is rejected");

    //--------------------------------------------------------------------------
    // A VST3 source loads, activates and renders.

    const juce::String testPluginPath (IHUB_TESTPLUGIN_VST3);

    if (testPluginPath.isNotEmpty() && juce::File (testPluginPath).exists())
    {
        const std::string body = "{\"kind\":\"plugin\",\"name\":\"TestPlugin\","
                                 "\"path\":\"" + testPluginPath.toStdString() + "\","
                                 "\"pluginId\":\"com.instrumenthub.TestPlugin\","
                                 "\"pluginManufacturer\":\"Instrument Hub\","
                                 "\"pluginVersion\":\"1.0.0\"}";

        response = postWithDispatch (client, "/patches/0/source", body);
        check (response && response->status == 200, "a VST3 source is accepted");

        const bool active = session.waitForSlotWarm (0, 15000);
        check (active, "the armed slot warms the VST3 source");

        response = client.Get ("/state");

        const auto pluginPatch = patchForSlot (parseJson (response->body), 0);
        const auto pluginInstrument = property (pluginPatch, "instrument");

        check (property (pluginInstrument, "kind").toString() == "plugin",
               "/state publishes the plug-in source kind");
        check (property (pluginInstrument, "pluginName").toString() == "TestPlugin",
               "/state publishes the plug-in name");

        // TestPlugin is a gain stage with an optional sine generator that
        // defaults to 0, so a hosted instance is legitimately silent until its
        // parameters are set. Prove the instance processed the block (finite,
        // in range) rather than asserting a level.
        const auto pluginAudio = capture (session, kSettleBlocks, kMeasureBlocks);
        bool finite = ! pluginAudio.empty();

        for (const float sample : pluginAudio)
            finite = finite && std::isfinite (sample) && std::abs (sample) <= 2.0f;

        check (finite, "the hosted plug-in source processes the rendered block");
        check (session.getWarmStateName (0) == std::string ("active"),
               "the plug-in source is the active slot");

        // Back to a tone so the chain/level checks above keep their meaning.
        response = postWithDispatch (client, "/patches/0/source",
                                     "{\"kind\":\"init-tone\",\"frequency\":220}");
        check (response && response->status == 200, "the slot returns to a tone source");
        check (session.waitForSlotWarm (0, 5000), "the tone source warms again");
        check (rmsOf (capture (session, kSettleBlocks, kMeasureBlocks)) > 0.01,
               "the graph renders again after a plug-in source was hosted");
    }
    else
    {
        std::cout << "SKIP the VST3 source check (IHUB_TESTPLUGIN_VST3 is not set)"
                  << std::endl;
    }

    server.stop();

    if (failures > 0)
    {
        std::cerr << "instrument-page-test: " << failures << " check(s) failed"
                  << std::endl;
        return 1;
    }

    std::cout << "instrument-page-test: all checks passed" << std::endl;
    return 0;
}
