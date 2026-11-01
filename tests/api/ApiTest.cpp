// T050 acceptance test: HTTP routes, token auth and the WebSocket meter rate.

#include "api/Server.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

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
} // namespace

int main()
{
    Api::StateStore state (8);
    state.setSlotLatency (2, 240, 48000.0);

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0; // ephemeral, so tests never fight over 8080
    options.meterHz = 30;

    Api::Server server (state, options);

    if (! server.start())
    {
        std::cerr << "api-test: FAILED to start the server" << std::endl;
        return 1;
    }

    std::cout << "api-test: listening on 127.0.0.1:" << server.getPort() << std::endl;

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (2, 0);

    // --- GET /state --------------------------------------------------------
    auto response = client.Get ("/state");
    check (response && response->status == 200, "GET /state returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (body.isObject(), "GET /state body is a JSON object");
        check (body.getDynamicObject()->getProperty ("schemaVersion").equals (1),
               "state schemaVersion is 1");
        check (static_cast<int> (body.getDynamicObject()->getProperty ("armedSlot")) == -1,
               "initial armedSlot is -1");

        const auto slots = body.getDynamicObject()->getProperty ("slots");

        if (const auto* array = slots.getArray())
        {
            check (array->size() == 8, "state lists 8 slots");

            const auto third = array->size() > 2 ? (*array)[2] : juce::var();
            check (third.isObject()
                       && static_cast<int> (third.getDynamicObject()
                                                ->getProperty ("latencySamples")) == 240,
                   "GET /state publishes the slot's 240-sample chain latency");
            check (third.isObject()
                       && std::abs (static_cast<double> (third.getDynamicObject()
                                                             ->getProperty ("latencyMs")) - 5.0)
                              < 1.0e-9,
                   "GET /state publishes the same chain latency as 5 ms");
        }
        else
        {
            check (false, "state slots is an array");
        }
    }

    // --- POST /arm ---------------------------------------------------------
    response = client.Post ("/arm", "{\"slot\":3}", "application/json");
    check (response && response->status == 200, "POST /arm returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (static_cast<int> (body.getDynamicObject()->getProperty ("armedSlot")) == 3,
               "POST /arm arms slot 3");
        check (state.getArmedSlot() == 3, "state store reports slot 3 armed");

        const auto slots = body.getDynamicObject()->getProperty ("slots").getArray();
        const auto third = slots != nullptr ? (*slots)[3] : juce::var();
        check (third.isObject()
                   && static_cast<bool> (third.getDynamicObject()->getProperty ("armed")),
               "slot 3 is marked armed");
    }

    response = client.Post ("/arm", "{\"slot\":99}", "application/json");
    check (response && response->status == 400, "POST /arm rejects an out-of-range slot");

    response = client.Post ("/arm", "{\"slot\":null}", "application/json");
    check (response && response->status == 200, "POST /arm accepts null to disarm");
    check (state.getArmedSlot() == -1, "state store disarmed after null");

    // --- Settings ----------------------------------------------------------
    response = client.Get ("/settings");
    check (response && response->status == 200, "GET /settings returns 200");

    response = client.Post ("/settings", "{\"masterGain\":0.5}", "application/json");
    check (response && response->status == 200, "POST /settings returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (std::abs (static_cast<double> (body.getDynamicObject()->getProperty ("masterGain"))
                         - 0.5) < 1.0e-9,
               "masterGain updated to 0.5");
    }

    check (std::abs (state.getSettings().masterGain - 0.5) < 1.0e-9,
           "state store holds masterGain 0.5");

    response = client.Post ("/settings", "{\"masterGain\":5.0}", "application/json");
    check (response && response->status == 400, "POST /settings rejects an out-of-range value");

    response = client.Post ("/settings", "{\"nonsense\":1}", "application/json");
    check (response && response->status == 400, "POST /settings rejects an unknown field");

    // --- Transport ---------------------------------------------------------
    response = client.Post ("/transport", "{\"playing\":true,\"bpm\":128}", "application/json");
    check (response && response->status == 200, "POST /transport returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (static_cast<bool> (body.getDynamicObject()->getProperty ("playing")),
               "transport is playing");
        check (std::abs (static_cast<double> (body.getDynamicObject()->getProperty ("bpm")) - 128.0)
                   < 1.0e-9,
               "transport bpm is 128");
    }

    response = client.Post ("/transport", "{\"bpm\":1000}", "application/json");
    check (response && response->status == 400, "POST /transport rejects an out-of-range bpm");

    // --- Every route module is wired in ------------------------------------
    //
    // The handlers live in api/routes/*. This walks one request per resource
    // and only asserts that a handler answered (anything but the server's
    // default 404), so a module dropped from registerRoutes() fails here.

    struct RouteProbe
    {
        const char* method;
        const char* path;
        const char* body;
    };

    const RouteProbe probes[] = {
        { "POST", "/arm", "{}" },
        { "PATCH", "/patches/0", "{}" },
        { "POST", "/patches/0/source", "{}" },
        { "POST", "/patches/0/editor", nullptr },
        { "DELETE", "/patches/0/editor", nullptr },
        { "POST", "/patches/0/state/capture", nullptr },
        { "GET", "/patches/0/fx", nullptr },
        { "POST", "/patches/0/fx", "{}" },
        { "PUT", "/patches/0/fx", "{}" },
        { "PATCH", "/patches/0/fx/0", "{}" },
        { "DELETE", "/patches/0/fx/0", nullptr },
        { "POST", "/patches/0/fx/reorder", "{}" },
        { "GET", "/settings", nullptr },
        { "POST", "/settings", "{}" },
        { "GET", "/transport", nullptr },
        { "GET", "/audio", nullptr },
        { "POST", "/audio", "{}" },
        { "POST", "/audio/measure", nullptr },
        { "GET", "/plugins", nullptr },
        { "POST", "/plugins/rescan", nullptr },
        { "GET", "/midi", nullptr },
        { "POST", "/midi/inputs", "{}" },
        { "POST", "/midi/pad-test", nullptr },
        { "GET", "/pairing", nullptr },
        { "POST", "/pairing/lan", "{}" },
        { "POST", "/pairing/revoke", nullptr },
        { "POST", "/pair", "{}" },
    };

    for (const auto& probe : probes)
    {
        const std::string method (probe.method);
        const std::string path (probe.path);
        const std::string body = probe.body != nullptr ? probe.body : std::string();
        httplib::Result result;

        if (method == "GET")
            result = client.Get (path);
        else if (method == "POST")
            result = client.Post (path, body, "application/json");
        else if (method == "PUT")
            result = client.Put (path, body, "application/json");
        else if (method == "PATCH")
            result = client.Patch (path, body, "application/json");
        else if (method == "DELETE")
            result = client.Delete (path);

        check (result && result->status != 404,
               method + " " + path + " is registered");
    }

    // --- Token auth --------------------------------------------------------
    server.setAuthToken ("test-token");

    response = client.Get ("/state");
    check (response && response->status == 401, "auth enabled: missing token gets 401");

    response = client.Get ("/state",
                           httplib::Headers { { "X-Instrument-Hub-Token", "test-token" } });
    check (response && response->status == 200, "auth enabled: header token gets 200");

    response = client.Get ("/state",
                           httplib::Headers { { "Authorization", "Bearer test-token" } });
    check (response && response->status == 200, "auth enabled: bearer token gets 200");

    server.setAuthToken ("");

    // --- WebSocket meters --------------------------------------------------
    httplib::ws::WebSocketClient websocket ("ws://127.0.0.1:"
                                            + std::to_string (server.getPort()) + "/meters");
    websocket.set_connection_timeout (2, 0);
    websocket.set_read_timeout (1, 0);

    const auto connected = websocket.connect();
    check (static_cast<bool> (connected), "WebSocket /meters connects");

    int frames = 0;
    bool framesValid = true;
    std::string firstFrame;

    if (connected)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (1500);
        std::string message;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto result = websocket.read (message);

            if (result == httplib::ws::ReadResult::Text)
            {
                const auto frame = parseJson (message);

                if (! frame.isObject()
                    || frame.getDynamicObject()->getProperty ("type").toString() != "meters"
                    || ! frame.getDynamicObject()->getProperty ("slots").isArray())
                {
                    framesValid = false;
                    break;
                }

                if (firstFrame.empty())
                    firstFrame = message;

                ++frames;
            }
            else if (result == httplib::ws::ReadResult::Fail)
            {
                break;
            }
        }
    }

    check (framesValid, "every meter frame is a valid JSON meter object");
    check (frames >= 15, "at least 10 meter frames per second (got "
                             + std::to_string (frames) + " in 1.5 s)");

    std::cout << "api-test: first frame: " << firstFrame << std::endl;

    websocket.close();
    server.stop();

    std::cout << (failures == 0 ? "api-test: PASS" : "api-test: FAILED")
              << " failures=" << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
