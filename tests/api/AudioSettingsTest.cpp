// T053 acceptance test: audio settings routes plus the real device bridge.
//
// The first half starts the control server against an in-memory audio backend
// and proves the request/response contract: GET /audio, POST /audio (buffer
// size and channel mapping), validation and POST /audio/measure.
//
// The second half drives AudioSettingsBridge against the ALSA Loopback card:
// open 48 kHz / 128, change the buffer to 256 and back, read the latency
// budget, remap a channel, and measure a plausible round trip.

#include "audio/AudioSettings.h"

#include "api/Server.h"

#include <httplib.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
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

double numberProperty (const juce::var& object, const juce::String& name)
{
    return static_cast<double> (object.getDynamicObject()->getProperty (name));
}

int intProperty (const juce::var& object, const juce::String& name)
{
    return static_cast<int> (object.getDynamicObject()->getProperty (name));
}

bool boolProperty (const juce::var& object, const juce::String& name)
{
    return static_cast<bool> (object.getDynamicObject()->getProperty (name));
}

void recalculate (Api::AudioStateSnapshot& audio)
{
    const auto rate = audio.sampleRate > 0.0 ? audio.sampleRate : 48000.0;

    audio.latency.bufferMs = static_cast<double> (audio.bufferSize) * 1000.0 / rate;
    audio.latency.totalMs = audio.latency.bufferMs
                            + audio.latency.deviceMs
                            + audio.latency.chainMs;
}

Api::AudioStateSnapshot makeFakeAudio()
{
    Api::AudioStateSnapshot audio;
    audio.open = true;
    audio.deviceType = "ALSA";
    audio.deviceName = "Loopback";
    audio.deviceTypes = { "ALSA" };
    audio.devices = { { "ALSA", "Loopback" } };
    audio.sampleRate = 48000.0;
    audio.bufferSize = 128;
    audio.sampleRates = { 44100.0, 48000.0, 96000.0 };
    audio.bufferSizes = { 64, 128, 256, 512 };

    for (int index = 0; index < 2; ++index)
    {
        audio.inputChannels.push_back ({ index,
                                         "channel " + std::to_string (index + 1),
                                         true });
        audio.outputChannels.push_back ({ index,
                                          "channel " + std::to_string (index + 1),
                                          true });
    }

    audio.latency.deviceMs = 1.5;
    audio.latency.chainMs = 4.0;
    recalculate (audio);

    audio.deviceXruns = 1;
    audio.callbackOverruns = 2;
    audio.xruns = 3;
    audio.callbacks = 4096;
    audio.loadPercent = 6.25;
    return audio;
}

void routeTest()
{
    Api::StateStore state (8);
    auto audio = makeFakeAudio();
    int measurements = 0;

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    Api::Server server (state, options);

    server.setAudioHandlers (
        [&audio] { return audio; },
        [&audio] (const Api::AudioSettingsPatch& patch, std::string& error)
        {
            if (patch.bufferSize.has_value()
                && (*patch.bufferSize < 16 || *patch.bufferSize > 8192))
            {
                error = "bufferSize must be between 16 and 8192";
                return false;
            }

            if (patch.sampleRate.has_value())
                audio.sampleRate = *patch.sampleRate;

            if (patch.bufferSize.has_value())
                audio.bufferSize = *patch.bufferSize;

            if (patch.deviceName.has_value())
                audio.deviceName = *patch.deviceName;

            if (patch.inputChannels.has_value())
            {
                for (auto& channel : audio.inputChannels)
                    channel.active = std::find (patch.inputChannels->begin(),
                                                patch.inputChannels->end(),
                                                channel.index) != patch.inputChannels->end();
            }

            if (patch.outputChannels.has_value())
            {
                for (auto& channel : audio.outputChannels)
                    channel.active = std::find (patch.outputChannels->begin(),
                                                patch.outputChannels->end(),
                                                channel.index) != patch.outputChannels->end();
            }

            recalculate (audio);
            return true;
        },
        [&audio, &measurements]
        {
            ++measurements;

            Api::AudioMeasurementState result;
            result.valid = true;
            result.roundTripSamples = audio.bufferSize * 2;
            result.roundTripMs = static_cast<double> (result.roundTripSamples)
                                 * 1000.0 / audio.sampleRate;
            result.expectedMs = result.roundTripMs;
            result.confidence = 0.95f;
            result.sampleRate = audio.sampleRate;
            result.bufferSize = audio.bufferSize;
            return result;
        });

    if (! server.start())
    {
        std::cerr << "audio-settings-test: FAILED to start the API server" << std::endl;
        ++failures;
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    // --- GET /audio --------------------------------------------------------
    auto response = client.Get ("/audio");
    check (response && response->status == 200, "GET /audio returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (boolProperty (body, "open"), "GET /audio reports an open device");
        check (intProperty (body, "bufferSize") == 128, "GET /audio reports 128 frames");
        check (std::abs (numberProperty (body.getDynamicObject()->getProperty ("latency"),
                                         "totalMs") - 8.1667) < 0.01,
               "GET /audio reports buffer + device + chain latency");
        check (intProperty (body, "xruns") == 3, "GET /audio reports the xrun total");
    }

    // --- POST /audio: buffer size -----------------------------------------
    response = client.Post ("/audio", "{\"bufferSize\":256}", "application/json");
    check (response && response->status == 200, "POST /audio accepts bufferSize 256");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (intProperty (body, "bufferSize") == 256,
               "POST /audio response reports 256 frames");
        check (std::abs (numberProperty (body.getDynamicObject()->getProperty ("latency"),
                                         "bufferMs") - 5.3333) < 0.01,
               "POST /audio response recomputes the buffer latency");
    }

    check (audio.bufferSize == 256, "the engine backend applied 256 frames");

    response = client.Post ("/audio", "{\"bufferSize\":0}", "application/json");
    check (response && response->status == 400, "POST /audio rejects an out-of-range buffer");

    response = client.Post ("/audio", "{}", "application/json");
    check (response && response->status == 400, "POST /audio rejects an empty patch");

    response = client.Post ("/audio", "{\"nonsense\":1}", "application/json");
    check (response && response->status == 400, "POST /audio rejects an unknown field");

    // --- POST /audio: channel mapping -------------------------------------
    response = client.Post ("/audio", "{\"outputChannels\":[0]}", "application/json");
    check (response && response->status == 200, "POST /audio accepts a channel map");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto channels = body.getDynamicObject()->getProperty ("outputChannels");
        const auto* array = channels.getArray();
        check (array != nullptr && array->size() == 2
                   && boolProperty ((*array)[0], "active")
                   && ! boolProperty ((*array)[1], "active"),
               "the response carries the new channel mapping");
    }

    // --- POST /audio/measure ----------------------------------------------
    response = client.Post ("/audio/measure", "", "application/json");
    check (response && response->status == 200, "POST /audio/measure returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (boolProperty (body, "valid"), "the measurement is valid");
        check (intProperty (body, "roundTripSamples") == 512,
               "the measurement reports two buffers of samples");
        check (std::abs (numberProperty (body, "roundTripMs") - 10.6667) < 0.01,
               "the measurement reports a plausible 10.67 ms");
    }

    check (measurements == 1, "the measurement handler ran exactly once");

    // --- No backend --------------------------------------------------------
    Api::Server bare (state, options);

    if (bare.start())
    {
        httplib::Client bareClient ("127.0.0.1", bare.getPort());
        const auto bareResponse = bareClient.Post ("/audio", "{\"bufferSize\":256}",
                                                   "application/json");
        check (bareResponse && bareResponse->status == 503,
               "POST /audio without a backend answers 503");
        bare.stop();
    }

    server.stop();
}

void bridgeTest()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AudioEngine::AudioSettingsBridge bridge;
    juce::String error;

    if (! bridge.openLoopback (&error))
    {
        std::cout << "FAIL could not open the ALSA Loopback: " << error << std::endl;
        ++failures;
        return;
    }

    auto initial = bridge.snapshot();
    check (initial.open, "the bridge opened a real device");
    check (juce::String (initial.deviceName).containsIgnoreCase ("Loopback"),
           "the bridge opened the Loopback device");
    check (initial.bufferSize == 128, "the bridge opens at 128 frames");
    check (std::abs (initial.sampleRate - 48000.0) < 0.5,
           "the bridge opens at 48 kHz");
    check (initial.outputChannels.size() >= 2
               && initial.outputChannels[0].active
               && initial.outputChannels[1].active,
           "the first two output channels are active");
    check (std::abs (initial.latency.bufferMs - 2.6667) < 0.01,
           "the initial buffer latency is 2.67 ms");

    // --- buffer 128 -> 256 ------------------------------------------------
    Api::AudioSettingsPatch bufferPatch;
    bufferPatch.bufferSize = 256;
    std::string patchError;

    check (bridge.apply (bufferPatch, patchError),
           "the bridge applies bufferSize 256");

    auto after = bridge.snapshot();
    check (after.bufferSize == 256, "the device is running at 256 frames");
    check (std::abs (after.latency.bufferMs - 5.3333) < 0.01,
           "the buffer latency readout updates to 5.33 ms");

    bufferPatch.bufferSize = 128;
    check (bridge.apply (bufferPatch, patchError),
           "the bridge returns the buffer to 128 frames");
    check (bridge.snapshot().bufferSize == 128, "the device is back at 128 frames");

    // --- channel mapping ---------------------------------------------------
    Api::AudioSettingsPatch channelPatch;
    channelPatch.outputChannels = std::vector<int> { 0 };
    check (bridge.apply (channelPatch, patchError),
           "the bridge applies a one-channel output map");

    auto remapped = bridge.snapshot();
    check (remapped.outputChannels[1].active == false,
           "output channel 2 is no longer active");
    check (remapped.outputChannels[0].active, "output channel 1 stays active");

    channelPatch.outputChannels = std::vector<int> { 0, 1 };
    check (bridge.apply (channelPatch, patchError),
           "the bridge restores the stereo output map");

    // --- measurement -------------------------------------------------------
    const auto measurement = bridge.measure();
    check (measurement.valid, "the loopback measurement is valid");
    check (measurement.sampleRate > 0.0 && measurement.bufferSize == 128,
           "the measurement uses the open device rate and buffer");
    check (measurement.roundTripSamples > 0,
           "the measurement reports a positive round trip");

    if (measurement.valid)
    {
        const int expected = measurement.bufferSize * 2;
        const int tolerance = measurement.bufferSize;

        std::cout << "bridge: measured " << measurement.roundTripSamples
                  << " samples (" << measurement.roundTripMs << " ms), expected "
                  << expected << " samples (" << measurement.expectedMs << " ms)"
                  << std::endl;

        check (std::abs (measurement.roundTripSamples - expected) <= tolerance,
               "the measured round trip is within one buffer of 2x buffer");
        check (measurement.roundTripMs > 0.0 && measurement.roundTripMs < 100.0,
               "the measured latency is plausible");
        check (measurement.confidence > 0.0f,
               "the measurement reported a correlation confidence");
    }

    bridge.close();
}

} // namespace

int main()
{
    routeTest();
    bridgeTest();

    std::cout << (failures == 0 ? "audio-settings-test: PASS"
                                : "audio-settings-test: FAILED")
              << " failures=" << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
