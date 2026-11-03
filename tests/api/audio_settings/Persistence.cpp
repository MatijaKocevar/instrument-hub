#include "Persistence.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "api/Server.h"
#include "audio/AudioSettings.h"
#include "settings/SettingsStore.h"

#include <httplib.h>
#include <juce_events/juce_events.h>

#include <iostream>

namespace ApiTest::AudioSettings
{

namespace
{
juce::File makeTemporaryDirectory()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-audio-persistence-test-"
                                              + juce::Uuid().toString());

    directory.createDirectory();
    return directory;
}
}

void runPersistence()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto directory = makeTemporaryDirectory();
    const auto file = directory.getChildFile ("settings.json");

    ihub::settings::SettingsStore store (file);
    check (store.load() == ihub::settings::LoadStatus::missing,
           "the persistence test starts from a missing settings file");

    AudioEngine::AudioSettingsBridge bridge;
    juce::String openError;

    if (! bridge.openLoopback (&openError))
    {
        std::cout << "FAIL could not open the ALSA Loopback: "
                  << openError << std::endl;
        ++failures;
        directory.deleteRecursively();
        return;
    }

    bridge.setSettingsStore (&store);

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
        bridge.close();
        directory.deleteRecursively();
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    auto response = client.Get ("/audio");
    check (response && response->status == 200,
           "GET /audio answers before the first save");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto restore = property (body, "restore");
        check (! boolProperty (restore, "usedStoredDevice"),
               "a missing settings file is not a stored-device restore");
        check (! boolProperty (restore, "fallback"),
               "a missing settings file is not a fallback");
        check (stringProperty (restore, "reason").empty(),
               "a missing settings file has no restore reason");
    }

    response = client.Post ("/audio",
                            "{\"deviceName\":\"No Such Device 9000\"}",
                            "application/json");
    check (response && response->status == 400,
           "POST /audio rejects an unknown device");
    check (! file.existsAsFile(),
           "a failed POST /audio creates no settings file");

    response = client.Post ("/audio", "{\"bufferSize\":0}", "application/json");
    check (response && response->status == 400,
           "POST /audio rejects an out-of-range buffer");
    check (! file.existsAsFile(),
           "a rejected POST /audio creates no settings file");

    response = client.Post ("/audio", "{\"bufferSize\":256}", "application/json");
    check (response && response->status == 200,
           "POST /audio accepts bufferSize 256");
    check (file.existsAsFile(),
           "a successful POST /audio writes the settings file");

    auto document = juce::var();
    const auto parse = juce::JSON::parse (file.loadFileAsString(), document);
    check (parse.wasOk(), "the saved settings file is valid JSON");
    const auto audio = property (document, "audio");
    const auto snapshot = bridge.snapshot();

    check (stringProperty (audio, "deviceType") == snapshot.deviceType,
           "the saved device type is the effective type");
    check (stringProperty (audio, "deviceName") == snapshot.deviceName,
           "the saved device name is the effective name");
    check (intProperty (audio, "sampleRate") == 48000,
           "the saved sample rate is the effective rate");
    check (intProperty (audio, "bufferSize") == 256,
           "the saved buffer size is the effective size");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto restore = property (body, "restore");
        check (boolProperty (restore, "usedStoredDevice"),
               "a successful apply reports the stored device");
        check (! boolProperty (restore, "fallback"),
               "a successful apply clears the fallback flag");
    }

    const auto saved = file.loadFileAsString();

    response = client.Post ("/audio",
                            "{\"deviceName\":\"No Such Device 9000\"}",
                            "application/json");
    check (response && response->status == 400,
           "POST /audio rejects the unknown device after a save");
    check (file.loadFileAsString() == saved,
           "a failed POST /audio leaves the settings file byte-identical");

    server.stop();
    bridge.close();
    directory.deleteRecursively();
}

}
