#include "DeviceSwitch.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "api/Server.h"
#include "audio/AudioSettings.h"

#include <httplib.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <string>

namespace ApiTest::AudioSettings
{

namespace
{
std::string findLoopbackName (const Api::AudioStateSnapshot& audio)
{
    for (const auto& device : audio.devices)
        if (device.available && device.type == "ALSA"
            && device.name.find ("Loopback") != std::string::npos)
        {
            return device.name;
        }

    return {};
}

bool allActive (const juce::var& channels)
{
    const auto* array = channels.getArray();

    if (array == nullptr || array->isEmpty())
        return false;

    for (const auto& channel : *array)
        if (! boolProperty (channel, "active"))
            return false;

    return true;
}
}

void runDeviceSwitch()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AudioEngine::AudioSettingsBridge bridge;
    const auto initial = bridge.snapshot();
    const auto loopback = findLoopbackName (initial);

    if (loopback.empty())
    {
        std::cout << "FAIL no available Loopback device was listed" << std::endl;
        ++failures;
        return;
    }

    Api::StateStore state (8);
    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    Api::Server server (state, options);
    server.setAudioHandlers (
        [&bridge] { return bridge.snapshot(); },
        [&bridge] (const Api::AudioSettingsPatch& patch, std::string& error)
        {
            return bridge.apply (patch, error);
        },
        [&bridge] { return bridge.measure(); });

    if (! server.start())
    {
        std::cerr << "audio-settings-test: FAILED to start the API server" << std::endl;
        ++failures;
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    const std::string body = "{\"deviceName\":\"" + loopback
                             + "\",\"sampleRate\":48000,\"bufferSize\":128}";
    auto response = client.Post ("/audio", body, "application/json");

    check (response && response->status == 200,
           "POST /audio with only deviceName opens the device");

    if (response && response->status == 200)
    {
        const auto audio = parseJson (response->body);
        check (boolProperty (audio, "open"),
               "the device-only request reports an open device");
        check (stringProperty (audio, "deviceName") == loopback,
               "the device-only request reports the selected device");
        check (intProperty (audio, "bufferSize") == 128,
               "the device-only request keeps the requested buffer size");
        check (std::abs (numberProperty (audio, "sampleRate") - 48000.0) < 0.5,
               "the device-only request keeps the requested sample rate");

        const auto latency = property (audio, "latency");
        check (std::abs (numberProperty (latency, "bufferMs") - 2.6667) < 0.01,
               "the device-only request returns the live buffer latency");
        check (numberProperty (latency, "totalMs")
                   >= numberProperty (latency, "bufferMs"),
               "the device-only request returns the live total latency");

        const auto outputs = property (audio, "outputChannels");
        const auto* outputArray = outputs.getArray();
        check (outputArray != nullptr && outputArray->size() >= 2
                   && boolProperty ((*outputArray)[0], "active")
                   && boolProperty ((*outputArray)[1], "active"),
               "the device-only request activates the first two outputs");
        check (allActive (property (audio, "inputChannels")),
               "the device-only request activates every reported input");
    }

    response = client.Post ("/audio", "{\"deviceName\":\"No Such Device 9000\"}",
                            "application/json");
    check (response && response->status == 400,
           "POST /audio rejects a disconnected device");

    if (response && response->status == 400)
        check (stringProperty (parseJson (response->body), "error")
                   == "the device is not connected",
               "a disconnected device reports that it is not connected");

    server.stop();
    bridge.close();
}

} // namespace ApiTest::AudioSettings
