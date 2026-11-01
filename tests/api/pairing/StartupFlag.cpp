#include "StartupFlag.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <iostream>

namespace ApiTest::Pairing
{

void runStartupFlag (Fixture& fixture)
{
    Api::Server::Options lanOptions = makeOptions();
    lanOptions.lanMode = true;

    Api::Server lanServer (fixture.state, lanOptions);

    if (! lanServer.start())
    {
        std::cerr << "pairing-test: FAILED to start the LAN-mode server" << std::endl;
        ++failures;
        return;
    }

    httplib::Client lanClient ("127.0.0.1", lanServer.getPort());
    lanClient.set_connection_timeout (2, 0);
    lanClient.set_read_timeout (2, 0);

    auto lanResponse = lanClient.Get ("/state");
    check (lanResponse && lanResponse->status == 401,
           "--lan starts with authentication enforced");

    lanResponse = lanClient.Get ("/pairing");
    check (lanResponse && lanResponse->status == 200
               && isSixDigits (property (parseJson (lanResponse->body), "code")),
           "--lan issues a pairing code at startup");

    lanServer.stop();
}

} // namespace ApiTest::Pairing
