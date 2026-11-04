#include "Wizard.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"
#include "Fixtures.h"

#include "api/Server.h"
#include "audio/settings/AudioProfiles.h"
#include "audio/settings/AudioSetupRecommendation.h"

#include <httplib.h>

#include <cmath>
#include <string>
#include <vector>

namespace ApiTest::AudioSettings
{

namespace
{
Api::AudioWizardState makeWizard()
{
    Api::AudioWizardState wizard;
    wizard.deviceType = "JACK";
    wizard.deviceName = "Rubix22 Pro";
    wizard.soundcard = "rubix22";
    wizard.profile = "pro-audio";
    wizard.sampleRate = 48000.0;
    wizard.inputChannels = { 0, 1 };
    wizard.outputChannels = { 0, 1 };
    return wizard;
}

Api::AudioProbeState makeProbe (double rate,
                                int buffer,
                                double latency,
                                int xruns,
                                bool opened = true)
{
    Api::AudioProbeState probe;
    probe.sampleRate = rate;
    probe.bufferSize = buffer;
    probe.latencyMs = latency;
    probe.xruns = static_cast<std::uint64_t> (xruns);
    probe.opened = opened;
    probe.stable = opened && xruns == 0;
    return probe;
}
}

void runWizardRecommendation()
{
    {
        auto wizard = makeWizard();
        wizard.candidates = {
            makeProbe (48000.0, 64, 6.62, 3),
            makeProbe (48000.0, 96, 7.29, 0),
            makeProbe (48000.0, 128, 7.96, 0)
        };

        AudioEngine::detail::finalizeAudioWizard (wizard, 48000.0);

        check (wizard.valid, "a bad 64-frame probe still yields a recommendation");
        check (wizard.bufferSize == 96
                   && std::abs (wizard.sampleRate - 48000.0) < 0.5,
               "the wizard recommends the lowest stable buffer (96 frames)");
        check (wizard.xruns == 0, "the recommendation reports zero xruns");
        check (wizard.candidates[1].recommended && ! wizard.candidates[0].recommended,
               "only the stable candidate is marked recommended");
        check (wizard.tradeoff.find ("64 frames produced 3 xrun") != std::string::npos,
               "the tradeoff names the failing buffer and its xruns");
        check (wizard.tradeoff.find ("96 frames") != std::string::npos,
               "the tradeoff names the stable buffer that won");
    }

    {
        auto wizard = makeWizard();
        wizard.candidates = {
            makeProbe (48000.0, 64, 6.62, 0),
            makeProbe (48000.0, 96, 7.29, 0),
            makeProbe (48000.0, 128, 7.96, 0)
        };

        AudioEngine::detail::finalizeAudioWizard (wizard, 48000.0);

        check (wizard.bufferSize == 64,
               "when every buffer is stable the lowest buffer wins");
        check (wizard.tradeoff.find ("Every tested buffer was stable")
                   != std::string::npos,
               "the tradeoff explains why the lowest buffer won");
    }

    {
        auto wizard = makeWizard();
        wizard.candidates = {
            makeProbe (48000.0, 64, 6.62, 5),
            makeProbe (48000.0, 96, 7.29, 2),
            makeProbe (48000.0, 128, 7.96, 1)
        };

        AudioEngine::detail::finalizeAudioWizard (wizard, 48000.0);

        check (wizard.valid && wizard.bufferSize == 128,
               "without a stable buffer the fewest-xrun setting is reported");
        check (wizard.xruns == 1, "the fallback recommendation keeps its xrun count");
        check (wizard.tradeoff.find ("xrun") != std::string::npos,
               "the fallback tradeoff still reports xruns");
    }

    {
        auto wizard = makeWizard();
        wizard.candidates = {
            makeProbe (48000.0, 64, 0.0, 0, false),
            makeProbe (48000.0, 96, 0.0, 0, false),
            makeProbe (48000.0, 128, 0.0, 0, false)
        };

        AudioEngine::detail::finalizeAudioWizard (wizard, 48000.0);

        check (! wizard.valid && wizard.error.find ("could be opened")
                   != std::string::npos,
               "when no candidate opens the wizard reports a readable error");
    }

    check (AudioEngine::detail::soundcardKeyFor ("Rubix22 Pro") == "rubix22"
               && AudioEngine::detail::soundcardKeyFor ("Rubix22") == "rubix22",
           "the ALSA and JACK names share one soundcard key");
    check (AudioEngine::detail::soundcardKeyFor ("Steinberg UR22mkII Pro")
               == "steinberg ur22mkii",
           "the Pro suffix is removed for a second interface");
}

void runWizardRoutes()
{
    Api::StateStore state (8);
    FakeBackend backend;

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    Api::Server server (state, options);
    backend.attach (server);

    bool applied = false;

    server.setAudioWizardHandlers (
        []
        {
            auto wizard = makeWizard();
            wizard.candidates = {
                makeProbe (48000.0, 64, 6.62, 3),
                makeProbe (48000.0, 96, 7.29, 0),
                makeProbe (48000.0, 128, 7.96, 0)
            };
            AudioEngine::detail::finalizeAudioWizard (wizard, 48000.0);
            return wizard;
        },
        [&applied] (std::string&)
        {
            applied = true;
            return true;
        });

    if (! server.start())
    {
        check (false, "the wizard API server starts");
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    auto response = client.Post ("/audio/wizard", "", "application/json");
    check (response && response->status == 200, "POST /audio/wizard returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (boolProperty (body, "valid"), "the wizard response is valid");
        check (intProperty (body, "bufferSize") == 96,
               "the wizard response carries the recommended buffer");
        check (stringProperty (body, "deviceName") == "Rubix22 Pro",
               "the wizard response names the probed device");
        check (arrayProperty (body, "candidates") != nullptr
                   && arrayProperty (body, "candidates")->size() == 3,
               "the wizard response carries every probed setting");
    }

    response = client.Post ("/audio/wizard/apply", "", "application/json");
    check (response && response->status == 200,
           "POST /audio/wizard/apply returns 200");
    check (applied, "the wizard apply handler ran");

    Api::Server bare (state, options);

    if (bare.start())
    {
        httplib::Client bareClient ("127.0.0.1", bare.getPort());
        const auto bareResponse = bareClient.Post ("/audio/wizard", "",
                                                   "application/json");
        check (bareResponse && bareResponse->status == 503,
               "POST /audio/wizard without a wizard answers 503");
        bare.stop();
    }

    server.stop();
}

}
