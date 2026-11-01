#pragma once

#include "api/Server.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace ApiTest::Pairing
{

inline Api::Server::Options makeOptions()
{
    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 5;
    return options;
}

bool isSixDigits (const juce::var& value);

using Response = httplib::Result;

Response retry (httplib::Client& client,
                const std::string& path,
                const httplib::Headers& headers,
                const std::function<bool (const Response&)>& predicate,
                int timeoutMs = 5000);

struct Fixture
{
    Api::StateStore state { 4 };
    Api::Server::Options options = makeOptions();
    Api::Server server { state, options };
    std::unique_ptr<httplib::Client> client;
    int port = 0;

    bool start();

    httplib::Client& http() noexcept { return *client; }
};

} // namespace ApiTest::Pairing
