// T121 acceptance test: the per-patch FX chain API drives the real audio path.
//
// Builds a small headless session, attaches the HTTP control server, arms a
// patch and then edits its insert chain entirely through the API:
//
//   - add a compressor (with look-ahead latency) and an EQ, and prove the
//     rendered audio changes;
//   - reorder the two inserts and prove the rendered audio changes again
//     (a compressor is non-linear, so it does not commute with the EQ);
//   - bypass an insert and watch its latency disappear from the chain total;
//   - set the wet/dry balance to zero and back;
//   - add, inspect and remove a hosted VST3 insert;
//   - replace the whole chain with PUT and clear it again;
//   - check the error statuses for bad slots, indices and fields.
//
// Everything is offline and deterministic: this test drives the render calls,
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

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSettleBlocks = 32;
constexpr int kMeasureBlocks = 64;

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

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

double rmsOf (const std::vector<float>& samples)
{
    if (samples.empty())
        return 0.0;

    double sum = 0.0;

    for (const float sample : samples)
        sum += static_cast<double> (sample) * static_cast<double> (sample);

    return std::sqrt (sum / static_cast<double> (samples.size()));
}

double dbfs (double linear)
{
    return 20.0 * std::log10 (juce::jmax (1.0e-12, linear));
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

double maxDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    const std::size_t count = std::min (a.size(), b.size());
    double maximum = 0.0;

    for (std::size_t index = 0; index < count; ++index)
        maximum = juce::jmax (maximum,
                              std::abs (static_cast<double> (a[index])
                                        - static_cast<double> (b[index])));

    return maximum;
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
    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "patch-fx-api-test: FAILED to build the session: "
                  << error << std::endl;
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
        std::cerr << "patch-fx-api-test: FAILED to start the server" << std::endl;
        return 1;
    }

    std::cout << "patch-fx-api-test: listening on 127.0.0.1:" << server.getPort()
              << " slots=" << session.getNumSlots() << std::endl;

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (10, 0);

    auto response = client.Post ("/arm", "{\"slot\":0}", "application/json");
    check (response && response->status == 200, "POST /arm 0 returns 200");
    check (session.waitForSlotActive (0, 5000),
           "slot 0 warms and becomes the active patch");

    juce::AudioBuffer<float> warmup (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 16; ++block)
        session.render (warmup, midi, kBlockSize);

    const auto baseline = capture (session, 0, kMeasureBlocks);
    const double baselineRms = rmsOf (baseline);
    check (baselineRms > 0.01, "the armed patch renders a non-silent baseline");

    // --- GET on an empty chain ---------------------------------------------
    response = client.Get ("/patches/0/fx");
    check (response && response->status == 200, "GET /patches/0/fx returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertCount (chain) == 0, "a fresh patch has an empty insert chain");
        check (chainLatency (chain) == 0, "an empty chain reports zero latency");
    }

    // --- Add a look-ahead compressor ---------------------------------------
    response = client.Post ("/patches/0/fx",
                            R"({"type":"compressor","parameters":{
                                   "thresholdDb":-45,"ratio":10,
                                   "attackMs":1,"releaseMs":50}})",
                            "application/json");
    check (response && response->status == 200, "POST a compressor insert returns 200");

    int compressorLatency = 0;

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        compressorLatency = chainLatency (chain);
        check (insertCount (chain) == 1, "the compressor was added to the chain");
        check (insertType (chain, 0) == "compressor",
               "GET reports the compressor at index 0");
        check (compressorLatency == 240,
               "the compressor's 5 ms look-ahead is reported as 240 samples");
    }

    const auto compressed = capture (session, kSettleBlocks, kMeasureBlocks);
    const double compressedRms = rmsOf (compressed);
    std::cout << "patch-fx-api-test: baseline=" << dbfs (baselineRms)
              << " dBFS compressed=" << dbfs (compressedRms)
              << " dBFS latency=" << compressorLatency << std::endl;

    check (compressedRms < baselineRms * 0.5,
           "adding the compressor changes the rendered audio (level drops)");

    // --- Add an EQ and reorder the two inserts -----------------------------
    response = client.Post ("/patches/0/fx",
                            R"({"type":"eq","parameters":{
                                   "midFrequencyHz":110,"midGainDb":12,"midQ":1.0}})",
                            "application/json");
    check (response && response->status == 200, "POST an eq insert returns 200");
    check (response && insertCount (parseJson (response->body)) == 2,
           "the chain now has two inserts");

    const auto compressorFirst = capture (session, kSettleBlocks, kMeasureBlocks);
    const double compressorFirstRms = rmsOf (compressorFirst);

    response = client.Post ("/patches/0/fx/reorder", R"({"order":[1,0]})",
                            "application/json");
    check (response && response->status == 200, "POST /fx/reorder returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertType (chain, 0) == "eq" && insertType (chain, 1) == "compressor",
               "reorder puts the eq before the compressor");
        check (static_cast<int> (property (chain, "latencySamples")) == 240,
               "reordering keeps the chain latency at 240 samples");
    }

    const auto eqFirst = capture (session, kSettleBlocks, kMeasureBlocks);
    const double eqFirstRms = rmsOf (eqFirst);
    std::cout << "patch-fx-api-test: compressor-first=" << dbfs (compressorFirstRms)
              << " dBFS eq-first=" << dbfs (eqFirstRms) << " dBFS" << std::endl;

    check (std::abs (dbfs (compressorFirstRms) - dbfs (eqFirstRms)) > 1.0,
           "reordering the chain changes the rendered audio");
    check (maxDifference (compressorFirst, eqFirst) > 1.0e-3,
           "the two orders produce different sample streams");

    // --- Bypass removes the compressor from the audible path --------------
    response = client.Patch ("/patches/0/fx/1", R"({"bypassed":true})",
                             "application/json");
    check (response && response->status == 200, "PATCH bypass returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (chainLatency (chain) == 0,
               "a bypassed insert contributes no latency");
        check (static_cast<bool> (property (
                   (*arrayProperty (chain, "inserts"))[1], "bypassed")),
               "GET reports the insert as bypassed");
    }

    const auto bypassed = capture (session, kSettleBlocks, kMeasureBlocks);
    check (rmsOf (bypassed) > eqFirstRms * 2.0,
           "bypassing the compressor restores the louder eq-only signal");

    response = client.Patch ("/patches/0/fx/1", R"({"bypassed":false})",
                             "application/json");
    check (response && response->status == 200, "PATCH unbypass returns 200");

    // --- Wet/dry ----------------------------------------------------------
    response = client.Patch ("/patches/0/fx/1", R"({"wet":0.0})",
                             "application/json");
    check (response && response->status == 200, "PATCH wet 0 returns 200");

    if (response && response->status == 200)
    {
        check (chainLatency (parseJson (response->body)) == 0,
               "a fully dry insert contributes no latency");
    }

    const auto dryOnly = capture (session, kSettleBlocks, kMeasureBlocks);
    check (rmsOf (dryOnly) > eqFirstRms * 2.0,
           "a fully dry compressor leaves the eq-only signal");

    response = client.Patch ("/patches/0/fx/1", R"({"wet":1.0})",
                             "application/json");
    check (response && response->status == 200, "PATCH wet 1 returns 200");

    // --- Move an insert with position -------------------------------------
    response = client.Patch ("/patches/0/fx/0", R"({"position":1})",
                             "application/json");
    check (response && response->status == 200, "PATCH position returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertType (chain, 0) == "compressor"
                   && insertType (chain, 1) == "eq",
               "PATCH position moves the eq after the compressor");
    }

    // --- Hosted VST3 insert ----------------------------------------------
    const std::string pluginPath = IHUB_TESTPLUGIN_VST3;

    if (! pluginPath.empty())
    {
        const std::string body = std::string (R"({"type":"vst3","path":")")
                                 + pluginPath + R"(","parameters":{"Gain":0.25}})";
        response = postWithDispatch (client, "/patches/0/fx", body);
        check (response && response->status == 200,
               "POST a hosted VST3 insert returns 200");

        if (! response || response->status != 200)
            std::cerr << "patch-fx-api-test: VST3 add failed: "
                      << (response ? response->body : std::string ("no response"))
                      << std::endl;

        if (response && response->status == 200)
        {
            const auto chain = parseJson (response->body);
            check (insertCount (chain) == 3, "the VST3 was added to the chain");
            check (insertType (chain, 2) == "vst3", "the VST3 insert is published");
            check (property ((*arrayProperty (chain, "inserts"))[2], "kind").toString()
                       == "vst3",
                   "the VST3 insert reports its kind");
        }

        response = requestWithDispatch ([&client]
        {
            return client.Delete ("/patches/0/fx/2");
        });
        check (response && response->status == 200, "DELETE the VST3 insert returns 200");
        check (response && insertCount (parseJson (response->body)) == 2,
               "the VST3 insert was removed");
    }
    else
    {
        std::cout << "patch-fx-api-test: no TestPlugin path; skipping the VST3 insert"
                  << std::endl;
    }

    // --- Replace the whole chain with PUT ---------------------------------
    const std::string replacement = R"({"chain":[
                                        {"type":"delay","wet":0.5,
                                         "parameters":{"tempoSync":0,"timeMs":50,
                                                       "feedback":0.5}}]})";
    response = requestWithDispatch ([&client, &replacement]
    {
        return client.Put ("/patches/0/fx", replacement, "application/json");
    });
    check (response && response->status == 200, "PUT /patches/0/fx returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertCount (chain) == 1 && insertType (chain, 0) == "delay",
               "PUT replaces the chain with the delay");
        check (std::abs (static_cast<double> (property (
                             (*arrayProperty (chain, "inserts"))[0], "wet")) - 0.5)
                   < 1.0e-9,
               "PUT carries the wet/dry balance");
        check (chainLatency (chain) == 0, "the delay reports no latency");
    }

    const auto delayed = capture (session, kSettleBlocks, kMeasureBlocks);
    check (maxDifference (baseline, delayed) > 1.0e-3,
           "the replacement chain changes the rendered audio");

    response = requestWithDispatch ([&client]
    {
        return client.Put ("/patches/0/fx", R"({"chain":[]})", "application/json");
    });
    check (response && response->status == 200, "PUT an empty chain returns 200");
    check (response && insertCount (parseJson (response->body)) == 0,
           "the chain is empty again");
    check (response && chainLatency (parseJson (response->body)) == 0,
           "the cleared chain reports zero latency");

    // --- Error handling ---------------------------------------------------
    response = client.Post ("/patches/0/fx", R"({"type":"nonsense"})",
                            "application/json");
    check (response && response->status == 400, "POST rejects an unknown effect type");

    response = client.Post ("/patches/0/fx", R"({"type":"eq","wet":1.5})",
                            "application/json");
    check (response && response->status == 400, "POST rejects an out-of-range wet value");

    response = client.Post ("/patches/0/fx", R"({"type":"eq","bogus":1})",
                            "application/json");
    check (response && response->status == 400, "POST rejects an unknown field");

    response = client.Get ("/patches/99/fx");
    check (response && response->status == 404, "GET rejects an out-of-range slot");

    response = client.Delete ("/patches/0/fx/0");
    check (response && response->status == 404, "DELETE rejects an out-of-range index");

    response = client.Patch ("/patches/0/fx/0", R"({"wet":0.5})",
                             "application/json");
    check (response && response->status == 404, "PATCH rejects an out-of-range index");

    response = client.Post ("/patches/0/fx/reorder", R"({"order":[0]})",
                            "application/json");
    check (response && response->status == 400,
           "reorder rejects an order that is not a permutation");

    server.stop();

    std::cout << (failures == 0 ? "patch-fx-api-test: PASS"
                                : "patch-fx-api-test: FAILED")
              << " failures=" << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
