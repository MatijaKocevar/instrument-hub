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
            const auto& keyboard = (*inputs)[2];
            const auto& grid = (*inputs)[3];

            check (stringProperty (launchpad, "role") == "launchpad",
                   "the first input is the Launchpad");
            check (stringProperty (launchpad, "port") == "MI",
                   "the Launchpad input names its MI port");
            check (boolProperty (launchpad, "enabled"), "the Launchpad starts enabled");
            check (boolProperty (launchpad, "handshake"),
                   "the Launchpad row carries the programmer-mode handshake");
            check (boolProperty (launchpad, "leds"),
                   "the Launchpad row reports LED output");
            check (stringProperty (keyboard, "role") == "keyboard",
                   "a generic keyboard carries the keyboard role");
            check (! boolProperty (keyboard, "enabled"), "the keyboard starts off");
            check (! boolProperty (grid, "leds"),
                   "a generic input reports no LED output");
            check (stringProperty ((*inputs)[1], "error").find ("DA port")
                       != std::string::npos,
                   "the unusable DA port explains itself");
        }

        check (! property (body, "keystation").isObject(),
               "the state no longer carries a Keystation card");

        const auto padTest = property (body, "padTest");

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
        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
        {
            const auto& row = (*inputs)[2];
            check (boolProperty (row, "enabled") && boolProperty (row, "open"),
                   "the keyboard row is enabled and open");
        }
    }

    response = client.Post ("/midi/inputs",
                            R"({"id":"ALSA:2:0","role":"grid","enabled":true})",
                            "application/json");
    check (response && response->status == 200,
           "POST /midi/inputs assigns the generic grid role");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
            check (stringProperty ((*inputs)[3], "role") == "grid",
                   "the generic input reports the grid role");
    }

    response = client.Post ("/midi/inputs",
                            R"({"id":"ALSA:2:0","noteBase":36})",
                            "application/json");
    check (response && response->status == 200,
           "POST /midi/inputs accepts a grid note base");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
            check (intProperty ((*inputs)[3], "noteBase") == 36,
                   "the grid row reports the note base");
    }

    response = client.Post ("/midi/inputs",
                            R"({"id":"ALSA:2:0","learn":true})",
                            "application/json");
    check (response && response->status == 200,
           "POST /midi/inputs starts a grid learn");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
            check (boolProperty ((*inputs)[3], "learnActive"),
                   "the grid row reports the running learn");
    }

    response = client.Post ("/midi/inputs",
                            R"({"id":"ALSA:2:0","role":"keyboard"})",
                            "application/json");
    check (response && response->status == 200,
           "POST /midi/inputs switches a grid back to a keyboard");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
            check (stringProperty ((*inputs)[3], "role") == "keyboard",
                   "the generic input reports the keyboard role again");
    }

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:9:9","enabled":true})",
                            "application/json");
    check (response && response->status == 404,
           "an unknown input identifier is rejected with 404");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0"})", "application/json");
    check (response && response->status == 400,
           "a body without a change is rejected");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0","enabled":"yes"})",
                            "application/json");
    check (response && response->status == 400, "a non-boolean enabled field is rejected");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0","noteBase":99})",
                            "application/json");
    check (response && response->status == 400, "a note base past 64 is rejected");

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
