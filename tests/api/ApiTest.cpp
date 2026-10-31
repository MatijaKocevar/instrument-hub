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
            check (array->size() == 8, "state lists 8 slots");
        else
            check (false, "state slots is an array");
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
