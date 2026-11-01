#include "Routes.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"
#include "Fixtures.h"

#include <httplib.h>

#include <iostream>

namespace ApiTest::MidiApi
{

void runRoutes()
{
    Api::StateStore state (8);
    FakeBackend backend;

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    Api::Server server (state, options);
    backend.attach (server);

    if (! server.start())
    {
        std::cerr << "midi-api-test: FAILED to start the API server" << std::endl;
        ++failures;
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    auto response = client.Get ("/midi");
    check (response && response->status == 200, "GET /midi returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto* inputs = property (body, "inputs").getArray();

        check (inputs != nullptr && inputs->size() == 4,
               "GET /midi lists all four ALSA inputs");
        check (intProperty (body, "inputCount") == 4, "the input count matches the list");

        if (inputs != nullptr && inputs->size() == 4)
        {
            const auto& launchpad = (*inputs)[0];
            const auto& keystation = (*inputs)[2];

            check (stringProperty (launchpad, "role") == "launchpad",
                   "the first input is the Launchpad");
            check (stringProperty (launchpad, "port") == "MI",
                   "the Launchpad input names its MI port");
            check (boolProperty (launchpad, "enabled"), "the Launchpad starts enabled");
            check (stringProperty (keystation, "role") == "keystation",
                   "the third input is the Keystation");
            check (! boolProperty (keystation, "enabled"), "the Keystation starts off");
        }

        const auto launchpad = property (body, "launchpad");
        const auto keystation = property (body, "keystation");
        const auto padTest = property (body, "padTest");

        check (stringProperty (launchpad, "port") == "MI",
               "the Launchpad reports the MI port");
        check (boolProperty (launchpad, "connected"), "the Launchpad is connected");
        check (boolProperty (launchpad, "handshake"),
               "the Launchpad reports its programmer-mode echo");
        check (stringProperty (keystation, "port") == "USB",
               "the Keystation reports its USB port");
        check (! boolProperty (padTest, "active"), "no pad test has run yet");
        check (intProperty (padTest, "durationMs") == Api::MidiBridge::padTestDurationMs,
               "the pad-test duration is published");
    }

    response = client.Post ("/midi/inputs",
                            R"({"id":"ALSA:5:0","enabled":true})",
                            "application/json");
    check (response && response->status == 200, "POST /midi/inputs accepts an enable");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto keystation = property (body, "keystation");

        check (boolProperty (keystation, "connected"),
               "the response reports the opened Keystation");

        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
        {
            const auto& row = (*inputs)[2];
            check (boolProperty (row, "enabled") && boolProperty (row, "open"),
                   "the Keystation row is enabled and open");
        }
    }

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:9:9","enabled":true})",
                            "application/json");
    check (response && response->status == 404,
           "an unknown input identifier is rejected with 404");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0"})", "application/json");
    check (response && response->status == 400, "a missing enabled field is rejected");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0","enabled":"yes"})",
                            "application/json");
    check (response && response->status == 400, "a non-boolean enabled field is rejected");

    response = client.Post ("/midi/pad-test", "", "application/json");
    check (response && response->status == 200, "POST /midi/pad-test returns 200");

    if (response && response->status == 200)
    {
        const auto padTest = property (parseJson (response->body), "padTest");

        check (boolProperty (padTest, "active"), "the pad test reports it is running");
        check (intProperty (padTest, "lit") == 64, "the pad test lights all 64 pads");
    }

    check (backend.padTests == 1, "the pad-test handler ran exactly once");

    Api::Server bare (state, options);

    if (bare.start())
    {
        httplib::Client bareClient ("127.0.0.1", bare.getPort());
        const auto empty = bareClient.Get ("/midi");

        check (empty && empty->status == 200,
               "GET /midi without a backend still answers 200");

        if (empty && empty->status == 200)
        {
            const auto* inputs = property (parseJson (empty->body), "inputs").getArray();
            check (inputs != nullptr && inputs->isEmpty(),
                   "the empty backend reports no inputs");
        }

        const auto toggle = bareClient.Post ("/midi/inputs",
                                             R"({"id":"x","enabled":true})",
                                             "application/json");
        check (toggle && toggle->status == 503,
               "POST /midi/inputs without a backend answers 503");

        const auto test = bareClient.Post ("/midi/pad-test", "", "application/json");
        check (test && test->status == 503,
               "POST /midi/pad-test without a backend answers 503");

        bare.stop();
    }

    server.stop();
}

}
