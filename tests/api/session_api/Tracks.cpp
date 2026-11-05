#include "Tracks.h"
#include "TrackSupport.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "Session.h"

#include <string>

namespace ApiTest::SessionApi
{

void runTracks()
{
    SessionRig rig (makeTrackConfig (kSlots), 5, "track-api-test");

    if (! rig.started())
    {
        check (false, "the track API rig starts");
        return;
    }

    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Get ("/state");
    auto state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "GET /state returns 200 for tracks");
    check (stringProperty (state, "layout") == "tracks",
           "GET /state reports the track layout");

    const auto* tracks = arrayProperty (state, "tracks");
    check (tracks != nullptr && tracks->size() == 8,
           "GET /state lists eight tracks");

    if (tracks != nullptr && tracks->size() == 8)
    {
        bool everyTrackHasFields = true;

        for (int index = 0; index < tracks->size(); ++index)
        {
            const auto track = (*tracks)[index];
            const auto instrument = property (track, "instrument");
            const auto* hot = arrayProperty (track, "hotFx");

            everyTrackHasFields = everyTrackHasFields
                                  && intProperty (track, "index") == index
                                  && intProperty (track, "slot") == index
                                  && property (track, "id").isString()
                                  && property (track, "name").isString()
                                  && property (track, "selected").isBool()
                                  && instrument.isObject()
                                  && property (instrument, "name").isString()
                                  && intProperty (track, "volumeStep") == 3
                                  && hot != nullptr && hot->size() == 2;
        }

        check (everyTrackHasFields,
               "every track carries index, slot, selection, instrument, volume and two hot FX");

        const auto first = (*tracks)[0];
        check (stringProperty (first, "id") == "track-1"
                   && stringProperty (first, "name") == "Track 1",
               "the first track carries its mapping id and name");
        check (stringProperty (property (first, "instrument"), "name") == "Track 1"
                   && stringProperty (property (first, "instrument"), "kind") == "init-tone",
               "the track instrument follows the slot source");
        check (stringProperty (hotFxAt (first, 0), "name") == "EQ"
                   && stringProperty (hotFxAt (first, 1), "name") == "Compressor",
               "hot FX entries carry the insert names");
        check (! boolProperty (hotFxAt (first, 0), "bypassed")
                   && ! boolProperty (hotFxAt (first, 1), "bypassed"),
               "hot FX entries report their bypass state");
    }

    httplib::ws::WebSocketClient events ("ws://127.0.0.1:"
                                         + std::to_string (rig.port())
                                         + "/events");
    events.set_connection_timeout (2, 0);
    events.set_read_timeout (2, 0);
    const auto eventsConnected = static_cast<bool> (events.connect());
    check (eventsConnected, "WebSocket /events connects for track commands");

    if (eventsConnected)
    {
        juce::var initial;

        if (readEventUntil (events, [] (const juce::var& frame)
        {
            return property (frame, "type").toString() == "state";
        }, 2000, initial))
        {
            check (property (initial, "state").isObject(),
                   "the track client receives the initial state frame");
        }
    }

    response = client.Post ("/tracks/2/select", "{}", "application/json");
    state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "POST /tracks/2/select returns 200");
    check (intProperty (state, "armedSlot") == kTrack,
           "track selection arms the track slot");
    check (session.getArmedSlot() == kTrack,
           "track selection arms the audio session");
    check (boolProperty (trackAt (state, kTrack), "selected"),
           "the selected track reports selected");
    check (! boolProperty (trackAt (state, 0), "selected"),
           "the previous track is no longer selected");

    const auto* slots = arrayProperty (state, "slots");

    if (slots != nullptr)
    {
        bool onlySelected = true;

        for (int index = 0; index < slots->size(); ++index)
            onlySelected = onlySelected
                           && boolProperty ((*slots)[index], "armed") == (index == kTrack);

        check (onlySelected, "exactly one slot is armed after track selection");
    }
    else
    {
        check (false, "the selected state still lists the slots");
    }

    if (eventsConnected)
    {
        juce::var frame;
        const bool sawTrack = readEventUntil (events, [&] (const juce::var& candidate)
        {
            return isTrackEvent (candidate, kTrack)
                   && boolProperty (trackAt (property (candidate, "state"), kTrack),
                                    "selected");
        }, 2000, frame);

        check (sawTrack, "the WebSocket delivers the track selection");
    }

    runTrackVolume (rig, events, eventsConnected);
    runTrackFx (rig, events, eventsConnected);

    response = client.Post ("/tracks/8/select", "{}", "application/json");
    check (response && response->status == 404,
           "track selection rejects an out-of-range track");

    rig.stop();
}

}
