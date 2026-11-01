#include "Meters.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <chrono>
#include <iostream>
#include <string>

namespace ApiTest::CoreApi
{

void runMeters (Rig& rig)
{
    httplib::ws::WebSocketClient websocket ("ws://127.0.0.1:"
                                            + std::to_string (rig.server.getPort())
                                            + "/meters");
    websocket.set_connection_timeout (2, 0);
    websocket.set_read_timeout (1, 0);

    const auto connected = websocket.connect();
    check (static_cast<bool> (connected), "WebSocket /meters connects");

    int frames = 0;
    bool framesValid = true;
    std::string firstFrame;

    if (connected)
    {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds (1500);
        std::string message;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto result = websocket.read (message);

            if (result == httplib::ws::ReadResult::Text)
            {
                const auto frame = parseJson (message);

                if (! frame.isObject()
                    || property (frame, "type").toString() != "meters"
                    || ! property (frame, "slots").isArray())
                {
                    framesValid = false;
                    break;
                }

                if (firstFrame.empty())
                    firstFrame = message;

                ++frames;
            }
            else if (result == httplib::ws::ReadResult::Fail)
            {
                break;
            }
        }
    }

    check (framesValid, "every meter frame is a valid JSON meter object");
    check (frames >= 15, "at least 10 meter frames per second (got "
                             + std::to_string (frames) + " in 1.5 s)");

    std::cout << "api-test: first frame: " << firstFrame << std::endl;

    websocket.close();
}

}
