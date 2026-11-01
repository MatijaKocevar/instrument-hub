#include "Fixtures.h"

#include <iostream>

namespace ApiTest::CoreApi
{

bool Rig::start()
{
    if (! server.start())
    {
        std::cerr << "api-test: FAILED to start the server" << std::endl;
        return false;
    }

    std::cout << "api-test: listening on 127.0.0.1:" << server.getPort() << std::endl;

    client = std::make_unique<httplib::Client> ("127.0.0.1", server.getPort());
    client->set_connection_timeout (2, 0);
    client->set_read_timeout (2, 0);
    return true;
}

} // namespace ApiTest::CoreApi
