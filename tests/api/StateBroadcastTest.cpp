// ihub-state-broadcast-test: the T103 acceptance check.
//
// The whole live-mirror path, without hardware:
//
//   LaunchpadDriver (the real note decoder and pad dispatch)
//     -> Api::StateBroadcast::publishPad()
//     -> /events WebSocket -> two connected clients
//
// Every event is timestamped on the test thread immediately before the note
// is injected, and again when a client receives the frame. The gap is the
// end-to-end latency the task contract caps at 100 ms.
//
// The same check is run for the UI path: `POST /arm` (what a grid tap does)
// must reach both sockets within 100 ms and carry the new armed slot.
//
// The MIDI leg uses the driver's injectMidiMessage() seam, so no ALSA
// sequencer port is opened and the test exercises the same decoder the MIDI
// thread uses.

#include "api/Server.h"
#include "api/StateBroadcast.h"

#include "controller/LaunchpadDriver.h"

#include <httplib.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr double kLatencyBudgetMs = 100.0;
constexpr int kPadsToTest = 4;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

double elapsedMs (std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli> (
               std::chrono::steady_clock::now() - start).count();
}

juce::var parseJson (const std::string& text)
{
    return juce::JSON::parse (juce::String (text));
}

juce::var property (const juce::var& object, const char* name)
{
    if (object.getDynamicObject() == nullptr)
        return {};

    return object.getDynamicObject()->getProperty (name);
}

/** Reads frames until `matches` accepts one or the timeout runs out. */
bool readUntil (httplib::ws::WebSocketClient& websocket,
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

bool isEvent (const juce::var& frame, const char* kind)
{
    return property (frame, "type").toString() == "state"
           && property (property (frame, "event"), "kind").toString() == kind;
}

/** `pads[n].pressed` from a state frame. */
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
            return static_cast<bool> (property (entry, "pressed"));
        }
    }

    return false;
}

struct Client
{
    explicit Client (int port)
        : socket ("ws://127.0.0.1:" + std::to_string (port) + "/events")
    {
        socket.set_connection_timeout (2, 0);
        socket.set_read_timeout (0, 50000); // 50 ms per read call
    }

    ~Client()
    {
        if (connected)
            socket.close();
    }

    bool connect()
    {
        connected = static_cast<bool> (socket.connect());
        return connected;
    }

    /** The frame every client gets on connect. */
    bool readInitial (juce::var& frame, int timeoutMs = 2000)
    {
        return readUntil (socket, [] (const juce::var& candidate)
        {
            return property (candidate, "type").toString() == "state";
        }, timeoutMs, frame);
    }

    httplib::ws::WebSocketClient socket;
    bool connected = false;
};

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    try
    {
        Api::StateStore state (8);

        Api::Server::Options options;
        options.address = "127.0.0.1";
        options.port = 0; // ephemeral: never fights another instance over 8080
        options.meterHz = 30;

        Api::Server server (state, options);

        if (! server.start())
        {
            std::cerr << "state-broadcast-test: FAILED to start the server" << std::endl;
            return 1;
        }

        auto& broadcast = server.getStateBroadcast();

        std::cout << "state-broadcast-test: api on 127.0.0.1:" << server.getPort()
                  << ", latency budget " << kLatencyBudgetMs << " ms" << std::endl;

        // ---------------------------------------------------------------------
        // Two clients, as the desktop UI and a tablet remote would be.
        // ---------------------------------------------------------------------
        Client desktop (server.getPort());
        Client tablet (server.getPort());

        check (desktop.connect(), "desktop WebSocket /events connects");
        check (tablet.connect(), "tablet WebSocket /events connects");

        juce::var initialDesktop;
        juce::var initialTablet;

        check (desktop.readInitial (initialDesktop),
               "desktop receives the initial state frame");
        check (tablet.readInitial (initialTablet),
               "tablet receives the initial state frame");

        if (initialDesktop.isObject())
        {
            check (isEvent (initialDesktop, "initial"),
                   "the first frame is marked initial");

            const auto pads = property (initialDesktop, "pads");

            check (pads.isArray() && pads.getArray()->size() == 64,
                   "the frame mirrors all 64 pads");

            const auto mirrored = property (initialDesktop, "state");
            check (mirrored.isObject()
                       && static_cast<int> (property (mirrored, "armedSlot")) == -1,
                   "the frame embeds the full /state document (nothing armed)");
        }

        // ---------------------------------------------------------------------
        // The real MIDI path: Launchpad note -> driver -> broadcast.
        // ---------------------------------------------------------------------
        Controller::LaunchpadDriver driver;

        std::atomic<int> padsSeen { 0 };

        Controller::LaunchpadDriver::DriverListeners listeners;
        listeners.padEvent = [&] (int pad, bool pressed)
        {
            broadcast.publishPad (pad, pressed);
            padsSeen.fetch_add (1, std::memory_order_release);
        };

        driver.setListeners (listeners);

        // ---------------------------------------------------------------------
        // Pad presses through the driver decoder.
        // ---------------------------------------------------------------------
        double worstPadLatencyMs = 0.0;

        for (int pad = 0; pad < kPadsToTest; ++pad)
        {
            const int note = Launchpad::padNoteForIndex (pad);
            const int expectedEvents = 2 * pad + 1;

            const auto pressedAt = std::chrono::steady_clock::now();
            driver.injectMidiMessage (juce::MidiMessage::noteOn (
                1, note, static_cast<juce::uint8> (100)));

            juce::var frame;

            const auto found = readUntil (desktop.socket, [&] (const juce::var& candidate)
            {
                return isEvent (candidate, "pad")
                       && static_cast<int> (property (property (candidate, "event"), "pad")) == pad
                       && static_cast<bool> (property (property (candidate, "event"), "pressed"));
            }, 1000, frame);

            const double latency = elapsedMs (pressedAt);
            worstPadLatencyMs = std::max (worstPadLatencyMs, latency);

            check (found, "pad " + std::to_string (pad) + " press reached the desktop socket");

            if (found)
            {
                check (latency <= kLatencyBudgetMs,
                       "pad " + std::to_string (pad) + " arrived in "
                           + std::to_string (latency) + " ms (<= 100 ms)");

                bool padFound = false;
                const bool mirrored = padPressed (frame, pad, padFound);
                check (padFound && mirrored,
                       "pad " + std::to_string (pad) + " is mirrored as pressed");

                check (static_cast<int> (property (frame, "sequence")) >= expectedEvents,
                       "the pad frame carries a fresh sequence number");
            }

            juce::var tabletFrame;
            const auto tabletFound = readUntil (tablet.socket, [&] (const juce::var& candidate)
            {
                return isEvent (candidate, "pad")
                       && static_cast<int> (property (property (candidate, "event"), "pad")) == pad
                       && static_cast<bool> (property (property (candidate, "event"), "pressed"));
            }, 1000, tabletFrame);

            const double tabletLatency = elapsedMs (pressedAt);
            worstPadLatencyMs = std::max (worstPadLatencyMs, tabletLatency);

            check (tabletFound && tabletLatency <= kLatencyBudgetMs,
                   "the same pad press reached the tablet socket in "
                       + std::to_string (tabletLatency) + " ms");

            check (padsSeen.load() >= expectedEvents,
                   "the driver delivered the pad event to the broadcaster");

            // Release, so the mirror tracks both edges. The Launchpad reports
            // a release as Note On velocity 0.
            driver.injectMidiMessage (juce::MidiMessage::noteOn (
                1, note, static_cast<juce::uint8> (0)));

            juce::var released;
            const auto releaseFound = readUntil (desktop.socket, [&] (const juce::var& candidate)
            {
                return isEvent (candidate, "pad")
                       && static_cast<int> (property (property (candidate, "event"), "pad")) == pad
                       && ! static_cast<bool> (property (property (candidate, "event"), "pressed"));
            }, 1000, released);

            check (releaseFound, "pad " + std::to_string (pad) + " release was broadcast");

            if (releaseFound)
            {
                bool padFound = false;
                check (! padPressed (released, pad, padFound) && padFound,
                       "pad " + std::to_string (pad) + " is mirrored as released");
            }
        }

        check (broadcast.getPad (kPadsToTest - 1).pressed == false,
               "the mirror keeps the final pad state");
        check (broadcast.getEventCount() >= 1 + 2 * kPadsToTest,
               "the broadcaster counted every published frame");

        // ---------------------------------------------------------------------
        // UI path: POST /arm is what a grid tap sends.
        // ---------------------------------------------------------------------
        httplib::Client http ("127.0.0.1", server.getPort());
        http.set_connection_timeout (2, 0);
        http.set_read_timeout (2, 0);

        const auto tappedAt = std::chrono::steady_clock::now();
        const auto response = http.Post ("/arm", "{\"slot\":5}", "application/json");

        const double postLatency = elapsedMs (tappedAt);

        check (response && response->status == 200, "POST /arm returns 200");

        juce::var armedDesktop;
        const auto armFound = readUntil (desktop.socket, [] (const juce::var& candidate)
        {
            return isEvent (candidate, "arm")
                   && static_cast<int> (property (property (candidate, "event"), "slot")) == 5;
        }, 1000, armedDesktop);

        const double armLatency = elapsedMs (tappedAt);
        worstPadLatencyMs = std::max (worstPadLatencyMs, armLatency);

        check (armFound, "the UI arm event reached the desktop socket");

        if (armFound)
        {
            check (armLatency <= kLatencyBudgetMs,
                   "the UI arm event arrived in " + std::to_string (armLatency)
                       + " ms (round trip incl. HTTP: " + std::to_string (postLatency)
                       + " ms)");

            const auto mirrored = property (armedDesktop, "state");
            check (static_cast<int> (property (mirrored, "armedSlot")) == 5,
                   "the mirrored state shows slot 5 armed");

            const auto slots = property (mirrored, "slots");

            if (const auto* array = slots.getArray())
            {
                check (array->size() == 8
                           && static_cast<bool> (property ((*array)[5], "armed")),
                       "slot 5 is the only newly armed slot in the frame");
            }
            else
            {
                check (false, "the mirrored state carries the slot list");
            }
        }

        juce::var armedTablet;
        const auto tabletArmFound = readUntil (tablet.socket, [] (const juce::var& candidate)
        {
            return isEvent (candidate, "arm")
                   && static_cast<int> (property (property (candidate, "event"), "slot")) == 5;
        }, 1000, armedTablet);

        check (tabletArmFound && elapsedMs (tappedAt) <= kLatencyBudgetMs,
               "the UI arm event reached the tablet socket within the budget");

        // ---------------------------------------------------------------------
        // Connect-time state: a client that joins later is brought up to date
        // by the next event, never by polling.
        // ---------------------------------------------------------------------
        check (broadcast.getClientCount() == 2,
               "the broadcaster reports both clients (got "
                   + std::to_string (broadcast.getClientCount()) + ")");
        check (broadcast.getFrameCount() >= broadcast.getEventCount(),
               "every published event produced at least one client frame");

        std::cout << "state-broadcast-test: worst latency "
                  << worstPadLatencyMs << " ms" << std::endl;

        server.stop();

        std::cout << (failures == 0 ? "PASS" : "FAIL")
                  << ": state broadcast acceptance (" << failures
                  << " failure(s))" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
