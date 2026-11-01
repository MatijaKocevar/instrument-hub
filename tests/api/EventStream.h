#pragma once

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <functional>

namespace ApiTest
{

bool readEventUntil (httplib::ws::WebSocketClient& websocket,
                     const std::function<bool (const juce::var&)>& matches,
                     int timeoutMs,
                     juce::var& result);

} // namespace ApiTest
