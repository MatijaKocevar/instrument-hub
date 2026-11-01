#pragma once

#include "api/Server.h"

#include <httplib.h>

#include <memory>

namespace ApiTest::CoreApi
{

inline Api::Server::Options makeOptions()
{
    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 30;
    return options;
}

struct Rig
{
    Api::StateStore state { 8 };
    Api::Server::Options options = makeOptions();
    Api::Server server { state, options };
    std::unique_ptr<httplib::Client> client;

    bool start();

    httplib::Client& http() noexcept { return *client; }
};

} // namespace ApiTest::CoreApi
