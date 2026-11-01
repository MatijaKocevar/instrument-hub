#include "Fixtures.h"

#include "../EventStream.h"
#include "../JsonSupport.h"

#include <iostream>

namespace ApiTest::StateBroadcast
{

double elapsedMs (std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli> (
               std::chrono::steady_clock::now() - start).count();
}

bool isEvent (const juce::var& frame, const char* kind)
{
    return property (frame, "type").toString() == "state"
           && property (property (frame, "event"), "kind").toString() == kind;
}

bool padPressed (const juce::var& frame, int pad, bool& found)
{
    found = false;

    const auto pads = property (frame, "pads");

    if (const auto* array = pads.getArray())
    {
        if (pad >= 0 && pad < array->size())
        {
            const auto entry = (*array)[pad];
            found = true;
            return boolProperty (entry, "pressed");
        }
    }

    return false;
}

Client::~Client()
{
    if (connected)
        socket.close();
}

bool Client::connect()
{
    connected = static_cast<bool> (socket.connect());
    return connected;
}

bool Client::readInitial (juce::var& frame, int timeoutMs)
{
    return readEventUntil (socket, [] (const juce::var& candidate)
    {
        return property (candidate, "type").toString() == "state";
    }, timeoutMs, frame);
}

bool Rig::start()
{
    if (! server.start())
    {
        std::cerr << "state-broadcast-test: FAILED to start the server" << std::endl;
        return false;
    }

    std::cout << "state-broadcast-test: api on 127.0.0.1:" << server.getPort()
              << ", latency budget " << kLatencyBudgetMs << " ms" << std::endl;

    desktop = std::make_unique<Client> (server.getPort());
    tablet = std::make_unique<Client> (server.getPort());
    return true;
}

void Rig::stop()
{
    server.stop();
}

}
