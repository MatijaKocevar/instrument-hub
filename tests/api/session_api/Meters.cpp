#include "Meters.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <httplib.h>

#include <chrono>
#include <iostream>
#include <string>

namespace ApiTest::SessionApi
{

void runMeters (SessionRig& rig)
{
    httplib::ws::WebSocketClient websocket ("ws://127.0.0.1:"
                                            + std::to_string (rig.port())
                                            + "/meters");
    websocket.set_connection_timeout (2, 0);
    websocket.set_read_timeout (1, 0);

    const auto connected = websocket.connect();
    check (static_cast<bool> (connected), "WebSocket /meters connects");

    if (connected)
    {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds (2000);

        bool sawPlayingSlot = false;
        bool sawSilentSlot = false;
        bool sawMaster = false;
        std::string lastFrame;

        while (std::chrono::steady_clock::now() < deadline)
        {
            std::string message;
            const auto result = websocket.read (message);

            if (result != httplib::ws::ReadResult::Text)
                continue;

            lastFrame = message;
            const auto frame = parseJson (message);
            const auto slotsVar = property (frame, "slots");
            const auto* slots = slotsVar.getArray();

            if (slots == nullptr || slots->size() < 2)
                continue;

            const double peak1 = numberProperty ((*slots)[1], "peak");
            const double peak0 = numberProperty ((*slots)[0], "peak");
            const double master = numberProperty (property (frame, "master"), "peak");

            sawPlayingSlot = sawPlayingSlot || peak1 > 0.01;
            sawSilentSlot = sawSilentSlot || peak0 < 0.001;
            sawMaster = sawMaster || master > 0.01;

            if (sawPlayingSlot && sawSilentSlot && sawMaster)
                break;
        }

        check (sawPlayingSlot,
               "/meters reports a non-zero peak for the playing slot");
        check (sawSilentSlot,
               "/meters reports silence for the idle slot");
        check (sawMaster, "/meters reports the master bus peak");
        std::cout << "session-api-test: last meter frame: " << lastFrame << std::endl;
    }

    websocket.close();
}

}
