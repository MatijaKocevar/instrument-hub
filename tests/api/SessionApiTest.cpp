// T115 acceptance test: the control API drives a real, headless session.
//
// Composes the default session offline (no audio device), attaches the HTTP +
// WebSocket server through Api::attachSession() and proves:
//
//   - POST /arm 0 then 1 changes the audible slot: the rendered tone moves
//     from slot 0's 110 Hz to slot 1's 123.47 Hz, the crossfade gains swap and
//     the previous slot goes idle;
//   - /meters reports a non-zero peak for the playing slot and zero for the
//     idle one, sampled from the real slot outputs and the master bus;
//   - GET /state reports the actual slot state/gain, not a synthetic mirror;
//   - POST /transport start/stop drives the session transport and its
//     sample-accurate position, while a stop does not rewind it.
//
// Everything is offline and deterministic: rendering is driven by this test,
// so the result does not depend on wall-clock timing or a sound card.

#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr double kSlot0Frequency = 110.0;
constexpr double kSlot1Frequency = 123.47;

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

/** Finds the patch with `slot` in a `/state` document's family tree. */
juce::var patchForSlot (const juce::var& state, int slot)
{
    const auto families = property (state, "families");

    if (const auto* familyArray = families.getArray())
    {
        for (const auto& family : *familyArray)
        {
            const auto patches = property (family, "patches");

            if (const auto* patchArray = patches.getArray())
            {
                for (const auto& patch : *patchArray)
                {
                    if (static_cast<int> (property (patch, "slot")) == slot)
                        return patch;
                }
            }
        }
    }

    return {};
}

/** Reads `/events` frames until `matches` accepts one or the timeout expires. */
bool readEventUntil (httplib::ws::WebSocketClient& websocket,
                     const std::function<bool (const juce::var&)>& matches,
                     int timeoutMs,
                     juce::var& result)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);
    std::string message;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto read = websocket.read (message);

        if (read == httplib::ws::ReadResult::Fail)
            return false;

        if (read != httplib::ws::ReadResult::Text)
            continue;

        auto frame = parseJson (message);

        if (! frame.isObject())
            continue;

        if (matches (frame))
        {
            result = frame;
            return true;
        }
    }

    return false;
}

void renderBlocks (ihub::Session& session, int blocks)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < blocks; ++block)
        session.render (buffer, midi, kBlockSize);
}

/** Renders `seconds` of the session and estimates the output frequency from
    upward zero crossings. The init tone is a clean sine, so this is enough to
    tell which slot is audible without a spectrum analyser.
*/
double captureFrequency (ihub::Session& session, double seconds)
{
    const int total = static_cast<int> (std::lround (seconds * kSampleRate / kBlockSize))
                      * kBlockSize;

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    std::vector<float> captured;
    captured.reserve (static_cast<std::size_t> (total));

    for (int done = 0; done < total; done += kBlockSize)
    {
        session.render (buffer, midi, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
            captured.push_back (buffer.getSample (0, frame));
    }

    int crossings = 0;

    for (std::size_t index = 1; index < captured.size(); ++index)
        if (captured[index - 1] <= 0.0f && captured[index] > 0.0f)
            ++crossings;

    return static_cast<double> (crossings) * kSampleRate
           / static_cast<double> (captured.size());
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
    config.slots = ihub::Session::makeDefaultSet();

    ihub::Session session;
    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "session-api-test: FAILED to build the session: "
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
        std::cerr << "session-api-test: FAILED to start the server" << std::endl;
        return 1;
    }

    std::cout << "session-api-test: listening on 127.0.0.1:" << server.getPort()
              << " slots=" << session.getNumSlots() << std::endl;

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    // --- POST /arm 0: slot 0 becomes the audible slot ----------------------
    auto response = client.Post ("/arm", "{\"slot\":0}", "application/json");
    check (response && response->status == 200, "POST /arm 0 returns 200");
    check (session.waitForSlotActive (0, 5000),
           "slot 0 warms and becomes the active patch");

    renderBlocks (session, 16); // let the 3 ms fade settle

    const double frequency0 = captureFrequency (session, 1.0);

    std::cout << "session-api-test: slot 0 frequency=" << frequency0
              << " Hz gain=" << session.getSwitcher().getSlotGain (0)
              << " peak=" << session.getSlotMeterPeak (0)
              << " master=" << session.getMasterMeterPeak() << std::endl;

    check (std::abs (frequency0 - kSlot0Frequency) < 2.0,
           "arming slot 0 makes its 110 Hz tone audible");
    check (session.getSwitcher().getSlotGain (0) > 0.99f,
           "SlotSwitcher faded slot 0 to full gain");
    check (session.getSwitcher().getSlotGain (1) < 0.01f,
           "SlotSwitcher is not playing slot 1");
    check (session.getSlotMeterPeak (0) > 0.01f,
           "slot 0 meter reads a non-zero peak from the real output");
    check (session.getMasterMeterPeak() > 0.01f,
           "master meter reads a non-zero peak from the real bus");

    auto stateResponse = client.Get ("/state");

    if (stateResponse && stateResponse->status == 200)
    {
        const auto body = parseJson (stateResponse->body);
        check (static_cast<int> (property (body, "armedSlot")) == 0,
               "GET /state reports slot 0 armed");

        const auto slotsVar = property (body, "slots");
        const auto* slots = slotsVar.getArray();
        const bool slot0Active = slots != nullptr && slots->size() > 0
                                 && property ((*slots)[0], "state").toString() == "active"
                                 && static_cast<double> (property ((*slots)[0], "gain")) > 0.99;
        check (slot0Active, "GET /state reports slot 0's real state \"active\" and gain 1");
    }
    else
    {
        check (false, "GET /state returns 200 while slot 0 is armed");
    }

    // --- POST /arm 1: the audible slot changes -----------------------------
    response = client.Post ("/arm", "{\"slot\":1}", "application/json");
    check (response && response->status == 200, "POST /arm 1 returns 200");
    check (session.waitForSlotActive (1, 5000),
           "slot 1 warms and becomes the active patch");

    renderBlocks (session, 16);

    const double frequency1 = captureFrequency (session, 1.0);

    std::cout << "session-api-test: slot 1 frequency=" << frequency1
              << " Hz gain=" << session.getSwitcher().getSlotGain (1)
              << " peak=" << session.getSlotMeterPeak (1)
              << " slot0_gain=" << session.getSwitcher().getSlotGain (0)
              << " master=" << session.getMasterMeterPeak() << std::endl;

    check (std::abs (frequency1 - kSlot1Frequency) < 2.0,
           "arming slot 1 changes the audible tone to 123.47 Hz");
    check (session.getSwitcher().getSlotGain (1) > 0.99f,
           "SlotSwitcher faded slot 1 to full gain");
    check (session.getSwitcher().getSlotGain (0) < 0.01f,
           "SlotSwitcher faded slot 0 out");
    check (session.getSlotMeterPeak (1) > 0.01f,
           "slot 1 meter reads a non-zero peak from the real output");

    stateResponse = client.Get ("/state");

    if (stateResponse && stateResponse->status == 200)
    {
        const auto body = parseJson (stateResponse->body);
        check (static_cast<int> (property (body, "armedSlot")) == 1,
               "GET /state reports slot 1 armed");

        const auto slotsVar = property (body, "slots");
        const auto* slots = slotsVar.getArray();

        const bool slot1Active = slots != nullptr && slots->size() > 1
                                 && property ((*slots)[1], "state").toString() == "active"
                                 && static_cast<double> (property ((*slots)[1], "gain")) > 0.99;
        const bool slot0Idle = slots != nullptr && slots->size() > 0
                               && property ((*slots)[0], "state").toString() == "idle"
                               && static_cast<double> (property ((*slots)[0], "gain"))
                                      < 0.01;

        check (slot1Active, "GET /state reports slot 1's real state \"active\"");
        check (slot0Idle, "GET /state reports slot 0's real state \"idle\"");
    }
    else
    {
        check (false, "GET /state returns 200 while slot 1 is armed");
    }

    // --- PATCH /patches/{slot}: mix, colour and /events (T120) -------------
    auto patchResponse = client.Get ("/state");

    if (patchResponse && patchResponse->status == 200)
    {
        const auto body = parseJson (patchResponse->body);
        const auto patch = patchForSlot (body, 1);

        check (patch.isObject(), "GET /state publishes slot 1 in the patch tree");
        check (property (patch, "instrument").isObject(),
               "the patch tree carries the instrument descriptor");
        check (property (patch, "warmState").toString() == "active",
               "the patch tree carries the live warm state");
        check (static_cast<int> (property (patch, "latencySamples")) >= 0
                   && property (patch, "latencyMs").isDouble(),
               "the patch tree carries chain latency");
        check (property (patch, "volumeDb").isDouble()
                   && property (patch, "pan").isDouble()
                   && property (patch, "mute").isBool()
                   && property (patch, "solo").isBool()
                   && property (patch, "colorIndex").isInt(),
               "the patch tree carries volume, pan, mute, solo and colour");
    }
    else
    {
        check (false, "GET /state returns 200 for the patch tree");
    }

    httplib::ws::WebSocketClient events ("ws://127.0.0.1:"
                                         + std::to_string (server.getPort())
                                         + "/events");
    events.set_connection_timeout (2, 0);
    events.set_read_timeout (1, 0);
    const auto eventsConnected = events.connect();
    check (static_cast<bool> (eventsConnected), "WebSocket /events connects");

    patchResponse = client.Patch ("/patches/1",
                                  "{\"volumeDb\":-12,\"colorIndex\":5}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 200,
           "PATCH /patches/1 returns 200");

    // -12 dB is 0.2512 linear; the fixed slot's audio gain is the proof that
    // the mixer reached the graph, not just the JSON document.
    const float expectedGain = static_cast<float> (std::pow (10.0, -12.0 / 20.0));
    check (std::abs (session.getEngine().getSlot (1).getGain() - expectedGain) < 0.001f,
           "PATCH volumeDb applies the linear gain to the audio slot");

    if (patchResponse && patchResponse->status == 200)
    {
        const auto body = parseJson (patchResponse->body);
        const auto patch = patchForSlot (body, 1);

        check (std::abs (static_cast<double> (property (patch, "volumeDb")) + 12.0)
                   < 1.0e-9,
               "PATCH volumeDb is published in the patch tree");
        check (static_cast<int> (property (patch, "colorIndex")) == 5,
               "PATCH colorIndex is published in the patch tree");
    }

    if (eventsConnected)
    {
        juce::var event;

        const bool sawPatch = readEventUntil (
            events,
            [] (const juce::var& frame)
            {
                return property (property (frame, "event"), "kind").toString() == "patch"
                       && static_cast<int> (property (property (frame, "event"), "slot")) == 1;
            },
            2000, event);

        check (sawPatch, "the /events WebSocket delivers the patch change");

        if (sawPatch)
        {
            const auto patch = patchForSlot (property (event, "state"), 1);
            check (std::abs (static_cast<double> (property (patch, "volumeDb")) + 12.0)
                       < 1.0e-9,
                   "the event frame carries the new patch volume");
            check (static_cast<int> (property (patch, "colorIndex")) == 5,
                   "the event frame carries the new patch colour");
        }
    }

    // Mute is resolved against the slot gain, so the audio goes silent.
    patchResponse = client.Patch ("/patches/1", "{\"mute\":true}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 200,
           "PATCH mute returns 200");
    check (session.getEngine().getSlot (1).getGain() < 1.0e-6f,
           "PATCH mute silences the audio slot");

    patchResponse = client.Patch ("/patches/1",
                                  "{\"mute\":false,\"volumeDb\":0,\"colorIndex\":0}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 200,
           "PATCH restores the patch mix");

    patchResponse = client.Patch ("/patches/999", "{\"volumeDb\":-3}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 404,
           "PATCH rejects an out-of-range patch slot");

    patchResponse = client.Patch ("/patches/1", "{\"colour\":3}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 400,
           "PATCH rejects an unknown patch field");

    patchResponse = client.Patch ("/patches/1", "{\"colorIndex\":99}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 400,
           "PATCH rejects an out-of-range colour");

    events.close();

    // --- /meters WebSocket reads the real engine peaks ---------------------
    httplib::ws::WebSocketClient websocket ("ws://127.0.0.1:"
                                            + std::to_string (server.getPort())
                                            + "/meters");
    websocket.set_connection_timeout (2, 0);
    websocket.set_read_timeout (1, 0);

    const auto connected = websocket.connect();
    check (static_cast<bool> (connected), "WebSocket /meters connects");

    if (connected)
    {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds (2000);

        bool sawPlayingSlot = false;
        bool sawSilentSlot = false;
        bool sawMaster = false;
        std::string lastFrame;

        while (std::chrono::steady_clock::now() < deadline)
        {
            std::string message;
            const auto result = websocket.read (message);

            if (result != httplib::ws::ReadResult::Text)
                continue;

            lastFrame = message;
            const auto frame = parseJson (message);
            const auto slotsVar = property (frame, "slots");
            const auto* slots = slotsVar.getArray();

            if (slots == nullptr || slots->size() < 2)
                continue;

            const double peak1 = static_cast<double> (property ((*slots)[1], "peak"));
            const double peak0 = static_cast<double> (property ((*slots)[0], "peak"));
            const double master = static_cast<double> (property (property (frame, "master"),
                                                                 "peak"));

            sawPlayingSlot = sawPlayingSlot || peak1 > 0.01;
            sawSilentSlot = sawSilentSlot || peak0 < 0.001;
            sawMaster = sawMaster || master > 0.01;

            if (sawPlayingSlot && sawSilentSlot && sawMaster)
                break;
        }

        check (sawPlayingSlot,
               "/meters reports a non-zero peak for the playing slot");
        check (sawSilentSlot,
               "/meters reports silence for the idle slot");
        check (sawMaster, "/meters reports the master bus peak");
        std::cout << "session-api-test: last meter frame: " << lastFrame << std::endl;
    }

    websocket.close();

    // --- POST /transport drives the session transport ----------------------
    response = client.Post ("/transport",
                            "{\"playing\":true,\"bpm\":120,\"positionSamples\":0}",
                            "application/json");
    check (response && response->status == 200, "POST /transport start returns 200");

    renderBlocks (session, 10);

    auto transportResponse = client.Get ("/transport");

    if (transportResponse && transportResponse->status == 200)
    {
        const auto body = parseJson (transportResponse->body);
        check (static_cast<bool> (property (body, "playing")),
               "GET /transport reports the session transport playing");
        check (static_cast<juce::int64> (property (body, "positionSamples")) == 1280,
               "GET /transport reports 10 rendered 128-frame blocks (1280 samples)");
        check (std::abs (static_cast<double> (property (body, "ppq")) - 1280.0 * 120.0
                                                                   / (60.0 * kSampleRate))
                   < 1.0e-9,
               "GET /transport reports the matching PPQ position");
    }
    else
    {
        check (false, "GET /transport returns 200 while playing");
    }

    response = client.Post ("/transport", "{\"playing\":false}", "application/json");
    check (response && response->status == 200, "POST /transport stop returns 200");

    renderBlocks (session, 10);

    transportResponse = client.Get ("/transport");

    if (transportResponse && transportResponse->status == 200)
    {
        const auto body = parseJson (transportResponse->body);
        check (! static_cast<bool> (property (body, "playing")),
               "GET /transport reports the session transport stopped");
        check (static_cast<juce::int64> (property (body, "positionSamples")) == 1280,
               "stopping the transport does not rewind the position");
    }

    // --- Out-of-range arm is still rejected -------------------------------
    response = client.Post ("/arm", "{\"slot\":99}", "application/json");
    check (response && response->status == 400, "POST /arm rejects an out-of-range slot");

    server.stop();

    std::cout << (failures == 0 ? "session-api-test: PASS" : "session-api-test: FAILED")
              << " failures=" << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
