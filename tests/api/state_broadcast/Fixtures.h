#pragma once

#include "api/Server.h"
#include "api/StateBroadcast.h"

#include "controller/LaunchpadDriver.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace ApiTest::StateBroadcast
{

constexpr double kLatencyBudgetMs = 100.0;
constexpr int kPadsToTest = 4;

double elapsedMs (std::chrono::steady_clock::time_point start);

bool isEvent (const juce::var& frame, const char* kind);

bool padPressed (const juce::var& frame, int pad, bool& found);

struct Client
{
    explicit Client (int port)
        : socket ("ws://127.0.0.1:" + std::to_string (port) + "/events")
    {
        socket.set_connection_timeout (2, 0);
        socket.set_read_timeout (0, 50000);
    }

    ~Client();

    bool connect();

    bool readInitial (juce::var& frame, int timeoutMs = 2000);

    httplib::ws::WebSocketClient socket;
    bool connected = false;
};

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
    std::unique_ptr<Client> desktop;
    std::unique_ptr<Client> tablet;
    Controller::LaunchpadDriver driver;
    std::atomic<int> padsSeen { 0 };
    double worstPadLatencyMs = 0.0;

    bool start();

    void stop();
};

} // namespace ApiTest::StateBroadcast
