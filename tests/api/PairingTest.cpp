// T101 acceptance test: LAN mode, 6-digit pairing codes and tokens.
//
// 1. With LAN mode off the loopback API needs no token.
// 2. Enabling LAN mode rebinds the listener, issues a 6-digit code and starts
//    answering unauthenticated requests with 401.
// 3. POST /pair with the code returns a token; that token grants /state.
// 4. Codes are single-use, tokens can be revoked, and turning LAN mode off
//    restores loopback-only unauthenticated access.

#include "api/Server.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <functional>
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

bool isSixDigits (const juce::var& value)
{
    if (! value.isString())
        return false;

    const auto text = value.toString();

    if (text.length() != 6)
        return false;

    for (int index = 0; index < text.length(); ++index)
        if (! juce::CharacterFunctions::isDigit (text[index]))
            return false;

    return true;
}

using Response = httplib::Result;

/** Retries until `predicate` accepts a response or the deadline passes. */
Response retry (httplib::Client& client,
                const std::string& path,
                const httplib::Headers& headers,
                const std::function<bool (const Response&)>& predicate,
                int timeoutMs = 5000)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto response = client.Get (path, headers);

        if (predicate (response))
            return response;

        std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }

    return client.Get (path, headers);
}
} // namespace

int main()
{
    Api::StateStore state (4);

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0; // ephemeral, so tests never fight over 8080
    options.meterHz = 5;

    Api::Server server (state, options);

    if (! server.start())
    {
        std::cerr << "pairing-test: FAILED to start the server" << std::endl;
        return 1;
    }

    const int port = server.getPort();
    std::cout << "pairing-test: listening on 127.0.0.1:" << port << std::endl;

    httplib::Client client ("127.0.0.1", port);
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (2, 0);

    // --- LAN mode is off by default ---------------------------------------
    auto response = client.Get ("/state");
    check (response && response->status == 200,
           "LAN off: unpaired GET /state returns 200 on loopback");

    response = client.Get ("/pairing");
    check (response && response->status == 200, "GET /pairing returns 200 on loopback");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (! static_cast<bool> (body.getDynamicObject()->getProperty ("lanEnabled")),
               "pairing state reports LAN mode off by default");
        check (body.getDynamicObject()->getProperty ("code").toString().isEmpty(),
               "no pairing code is issued while LAN mode is off");
        check (body.getDynamicObject()->getProperty ("payload").toString().isEmpty(),
               "no QR payload is issued while LAN mode is off");
    }

    response = client.Post ("/pair", "{\"code\":\"123456\"}", "application/json");
    check (response && response->status == 403,
           "POST /pair is refused while LAN mode is off");

    response = client.Post ("/pairing/lan", "{}", "application/json");
    check (response && response->status == 400,
           "POST /pairing/lan requires an \"enabled\" field");

    response = client.Post ("/pairing/lan", "{\"enabled\":\"yes\"}", "application/json");
    check (response && response->status == 400,
           "POST /pairing/lan rejects a non-boolean toggle");

    // --- Enable LAN mode ---------------------------------------------------
    const auto toggle = client.Post ("/pairing/lan", "{\"enabled\":true}", "application/json");
    check (toggle && toggle->status == 200, "POST /pairing/lan enables LAN mode");

    std::string code;
    std::string payload;

    if (toggle && toggle->status == 200)
    {
        const auto body = parseJson (toggle->body);
        const auto* object = body.getDynamicObject();

        check (static_cast<bool> (object->getProperty ("lanEnabled")),
               "toggle response reports LAN mode on");
        check (isSixDigits (object->getProperty ("code")),
               "toggle response contains a 6-digit pairing code");

        code = object->getProperty ("code").toString().toStdString();
        payload = object->getProperty ("payload").toString().toStdString();

        check (payload.find ("http://") == 0, "QR payload is an http URL");
        check (payload.find ("/remote?code=" + code) != std::string::npos,
               "QR payload carries the pairing code on the /remote route");
        check (payload.find (":" + std::to_string (port)) != std::string::npos,
               "QR payload advertises the listener's port");
        check (static_cast<double> (object->getProperty ("codeExpiresInMs")) > 0.0,
               "pairing code has a positive lifetime");
    }
    else
    {
        check (false, "could not read the pairing code");
    }

    // The listener rebinds asynchronously; the same port must come back with
    // authentication enforced.
    response = retry (client, "/state", {},
                      [] (const Response& candidate)
                      {
                          return candidate && candidate->status == 401;
                      });
    check (response && response->status == 401,
           "LAN on: unpaired GET /state gets 401");
    check (server.isLanModeEnabled(), "server reports LAN mode enabled");
    check (server.getAddress() == "0.0.0.0", "LAN mode rebinds the listener to all interfaces");
    check (server.getPort() == port, "the rebind keeps the same port");

    response = client.Get ("/state", httplib::Headers { { "X-Instrument-Hub-Token", "bogus" } });
    check (response && response->status == 401, "an unknown token gets 401");

    // --- Pairing -----------------------------------------------------------
    response = client.Post ("/pair", "{\"code\":\"000000\"}", "application/json");
    check (response && response->status == 401, "POST /pair rejects a wrong code");

    response = client.Post ("/pair", "{\"code\":\"12x45\"}", "application/json");
    check (response && response->status == 401, "POST /pair rejects a malformed code");

    response = client.Post ("/pair",
                            "{\"code\":\"" + code + "\",\"clientName\":\"Test tablet\"}",
                            "application/json");
    check (response && response->status == 200, "POST /pair accepts the 6-digit code");

    std::string token;

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        token = body.getDynamicObject()->getProperty ("token").toString().toStdString();

        check (token.size() == 64, "pairing returns a 64-character token");
        check (static_cast<int> (body.getDynamicObject()->getProperty ("pairedClients")) == 1,
               "pairing reports one paired client");
    }
    else
    {
        check (false, "could not read the pairing token");
    }

    response = client.Get ("/state",
                           httplib::Headers { { "X-Instrument-Hub-Token", token } });
    check (response && response->status == 200, "the pairing token grants GET /state");

    response = client.Get ("/state",
                           httplib::Headers { { "Authorization", "Bearer " + token } });
    check (response && response->status == 200, "the pairing token works as a Bearer token");

    // Single-use code: the same code cannot be replayed.
    response = client.Post ("/pair", "{\"code\":\"" + code + "\"}", "application/json");
    check (response && response->status == 401, "a used pairing code cannot be replayed");

    response = client.Get ("/pairing");
    check (response && response->status == 200, "GET /pairing stays available on loopback");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto rotated = body.getDynamicObject()->getProperty ("code").toString();

        check (rotated.length() == 6 && rotated != juce::String (code),
               "a successful pairing rotates the code");
        check (static_cast<int> (body.getDynamicObject()->getProperty ("pairedClients")) == 1,
               "pairing state reports the paired client");
    }

    // --- Revoke ------------------------------------------------------------
    response = client.Post ("/pairing/revoke", "", "application/json");
    check (response && response->status == 200, "POST /pairing/revoke returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (static_cast<int> (body.getDynamicObject()->getProperty ("pairedClients")) == 0,
               "revoking forgets every paired client");
    }

    response = client.Get ("/state",
                           httplib::Headers { { "X-Instrument-Hub-Token", token } });
    check (response && response->status == 401, "a revoked token gets 401");

    // --- Back to loopback-only --------------------------------------------
    response = client.Post ("/pairing/lan", "{\"enabled\":false}", "application/json");
    check (response && response->status == 200, "POST /pairing/lan disables LAN mode");

    response = retry (client, "/state", {},
                      [] (const Response& candidate)
                      {
                          return candidate && candidate->status == 200;
                      });
    check (response && response->status == 200,
           "LAN off: GET /state works without a token again");
    check (! server.isLanModeEnabled(), "server reports LAN mode disabled");
    check (server.getAddress() == "127.0.0.1", "LAN mode off rebinds to loopback");
    check (server.getPort() == port, "disabling LAN mode keeps the same port");

    response = client.Get ("/pairing");
    check (response && response->status == 200
               && parseJson (response->body)
                      .getDynamicObject()->getProperty ("code").toString().isEmpty(),
           "turning LAN mode off clears the pairing code");

    server.stop();

    // --- Startup flag ------------------------------------------------------
    Api::Server::Options lanOptions;
    lanOptions.address = "127.0.0.1";
    lanOptions.port = 0;
    lanOptions.meterHz = 5;
    lanOptions.lanMode = true;

    Api::Server lanServer (state, lanOptions);

    if (! lanServer.start())
    {
        std::cerr << "pairing-test: FAILED to start the LAN-mode server" << std::endl;
        return 1;
    }

    httplib::Client lanClient ("127.0.0.1", lanServer.getPort());
    lanClient.set_connection_timeout (2, 0);
    lanClient.set_read_timeout (2, 0);

    auto lanResponse = lanClient.Get ("/state");
    check (lanResponse && lanResponse->status == 401,
           "--lan starts with authentication enforced");

    lanResponse = lanClient.Get ("/pairing");
    check (lanResponse && lanResponse->status == 200
               && isSixDigits (parseJson (lanResponse->body)
                                   .getDynamicObject()->getProperty ("code")),
           "--lan issues a pairing code at startup");

    lanServer.stop();

    std::cout << (failures == 0 ? "pairing-test: PASS" : "pairing-test: FAILED")
              << " failures=" << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
