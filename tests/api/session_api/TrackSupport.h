#pragma once

#include "Fixtures.h"

#include "../JsonSupport.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

namespace ApiTest::SessionApi
{

constexpr int kTrack = 2;

inline juce::var trackAt (const juce::var& state, int index)
{
    const auto* tracks = property (state, "tracks").getArray();

    if (tracks == nullptr || index < 0 || index >= tracks->size())
        return {};

    return (*tracks)[index];
}

inline juce::var hotFxAt (const juce::var& track, int hot)
{
    const auto* entries = property (track, "hotFx").getArray();

    if (entries == nullptr || hot < 0 || hot >= entries->size())
        return {};

    return (*entries)[hot];
}

inline bool isTrackEvent (const juce::var& frame, int index)
{
    return property (property (frame, "event"), "kind").toString() == "track"
           && intProperty (property (frame, "event"), "slot") == index;
}

inline double expectedGain (int step)
{
    static constexpr double gains[] = { 0.0, 0.5, 0.75, 1.0,
                                        1.4125375446227544 };
    return gains[step];
}

void runTrackVolume (SessionRig& rig,
                     httplib::ws::WebSocketClient& events,
                     bool eventsConnected);

void runTrackFx (SessionRig& rig,
                 httplib::ws::WebSocketClient& events,
                 bool eventsConnected);

}
