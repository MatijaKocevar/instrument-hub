#include "EventStream.h"

#include "JsonSupport.h"

#include <chrono>
#include <string>

namespace ApiTest
{

bool readEventUntil (httplib::ws::WebSocketClient& websocket,
                     const std::function<bool (const juce::var&)>& matches,
                     int timeoutMs,
                     juce::var& result)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);
    std::string message;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto read = websocket.read (message);

        if (read == httplib::ws::ReadResult::Fail)
            return false;

        if (read != httplib::ws::ReadResult::Text)
            continue;

        auto frame = parseJson (message);

        if (! frame.isObject())
            continue;

        if (matches (frame))
        {
            result = frame;
            return true;
        }
    }

    return false;
}

}
