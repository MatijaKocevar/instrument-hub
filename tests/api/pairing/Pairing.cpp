#include "Pairing.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <string>

namespace ApiTest::Pairing
{

void runPairing (Fixture& fixture)
{
    auto& client = fixture.http();
    const int port = fixture.port;

    const auto toggle = client.Post ("/pairing/lan", "{\"enabled\":true}", "application/json");
    check (toggle && toggle->status == 200, "POST /pairing/lan enables LAN mode");

    std::string code;
    std::string payload;

    if (toggle && toggle->status == 200)
    {
        const auto body = parseJson (toggle->body);
        const auto* object = body.getDynamicObject();

        check (boolProperty (body, "lanEnabled"),
               "toggle response reports LAN mode on");
        check (isSixDigits (property (body, "code")),
               "toggle response contains a 6-digit pairing code");

        code = object->getProperty ("code").toString().toStdString();
        payload = object->getProperty ("payload").toString().toStdString();

        check (payload.find ("http://") == 0, "QR payload is an http URL");
        check (payload.find ("/remote?code=" + code) != std::string::npos,
               "QR payload carries the pairing code on the /remote route");
        check (payload.find (":" + std::to_string (port)) != std::string::npos,
               "QR payload advertises the listener's port");
        check (numberProperty (body, "codeExpiresInMs") > 0.0,
               "pairing code has a positive lifetime");
    }
    else
    {
        check (false, "could not read the pairing code");
    }

    auto response = retry (client, "/state", {},
                           [] (const Response& candidate)
                           {
                               return candidate && candidate->status == 401;
                           });
    check (response && response->status == 401,
           "LAN on: unpaired GET /state gets 401");
    check (fixture.server.isLanModeEnabled(), "server reports LAN mode enabled");
    check (fixture.server.getAddress() == "0.0.0.0",
           "LAN mode rebinds the listener to all interfaces");
    check (fixture.server.getPort() == port, "the rebind keeps the same port");

    response = client.Get ("/state", httplib::Headers { { "X-Instrument-Hub-Token", "bogus" } });
    check (response && response->status == 401, "an unknown token gets 401");

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
        token = stringProperty (body, "token");

        check (token.size() == 64, "pairing returns a 64-character token");
        check (intProperty (body, "pairedClients") == 1,
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

    response = client.Post ("/pair", "{\"code\":\"" + code + "\"}", "application/json");
    check (response && response->status == 401, "a used pairing code cannot be replayed");

    response = client.Get ("/pairing");
    check (response && response->status == 200, "GET /pairing stays available on loopback");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto rotated = stringProperty (body, "code");

        check (rotated.length() == 6 && rotated != code,
               "a successful pairing rotates the code");
        check (intProperty (body, "pairedClients") == 1,
               "pairing state reports the paired client");
    }

    response = client.Post ("/pairing/revoke", "", "application/json");
    check (response && response->status == 200, "POST /pairing/revoke returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (intProperty (body, "pairedClients") == 0,
               "revoking forgets every paired client");
    }

    response = client.Get ("/state",
                           httplib::Headers { { "X-Instrument-Hub-Token", token } });
    check (response && response->status == 401, "a revoked token gets 401");

    response = client.Post ("/pairing/lan", "{\"enabled\":false}", "application/json");
    check (response && response->status == 200, "POST /pairing/lan disables LAN mode");

    response = retry (client, "/state", {},
                      [] (const Response& candidate)
                      {
                          return candidate && candidate->status == 200;
                      });
    check (response && response->status == 200,
           "LAN off: GET /state works without a token again");
    check (! fixture.server.isLanModeEnabled(), "server reports LAN mode disabled");
    check (fixture.server.getAddress() == "127.0.0.1", "LAN mode off rebinds to loopback");
    check (fixture.server.getPort() == port, "disabling LAN mode keeps the same port");

    response = client.Get ("/pairing");
    check (response && response->status == 200
               && stringProperty (parseJson (response->body), "code").empty(),
           "turning LAN mode off clears the pairing code");

    fixture.server.stop();
}

}
