#include "Routes.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"
#include "Fixtures.h"

#include <httplib.h>

#include <cmath>
#include <iostream>

namespace ApiTest::AudioSettings
{

void runRoutes()
{
    Api::StateStore state (8);
    FakeBackend backend;
    backend.audio.restore = { true, true, "the stored device is not available" };

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    Api::Server server (state, options);
    backend.attach (server);

    if (! server.start())
    {
        std::cerr << "audio-settings-test: FAILED to start the API server" << std::endl;
        ++failures;
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

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

        const auto restore = property (body, "restore");
        check (boolProperty (restore, "usedStoredDevice"),
               "GET /audio reports that the stored device was used");
        check (boolProperty (restore, "fallback"),
               "GET /audio reports the restore fallback");
        check (stringProperty (restore, "reason")
                   == "the stored device is not available",
               "GET /audio explains why the restore fell back");
    }

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

    check (backend.audio.bufferSize == 256, "the engine backend applied 256 frames");

    response = client.Post ("/audio", "{\"bufferSize\":0}", "application/json");
    check (response && response->status == 400, "POST /audio rejects an out-of-range buffer");

    response = client.Post ("/audio", "{}", "application/json");
    check (response && response->status == 400, "POST /audio rejects an empty patch");

    response = client.Post ("/audio", "{\"nonsense\":1}", "application/json");
    check (response && response->status == 400, "POST /audio rejects an unknown field");

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

    check (backend.measurements == 1, "the measurement handler ran exactly once");

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

}
