#include "PadPath.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <algorithm>
#include <string>

namespace ApiTest::StateBroadcast
{

void runPadPath (Rig& rig)
{
    auto& broadcast = rig.server.getStateBroadcast();
    auto& desktop = *rig.desktop;
    auto& tablet = *rig.tablet;

    Controller::LaunchpadDriver::DriverListeners listeners;
    listeners.padEvent = [&rig, &broadcast] (int pad, bool pressed)
    {
        broadcast.publishPad (pad, pressed);
        rig.padsSeen.fetch_add (1, std::memory_order_release);
    };

    rig.driver.setListeners (listeners);

    for (int pad = 0; pad < kPadsToTest; ++pad)
    {
        const int note = Launchpad::padNoteForIndex (pad);
        const int expectedEvents = 2 * pad + 1;

        const auto pressedAt = std::chrono::steady_clock::now();
        rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (
            1, note, static_cast<juce::uint8> (100)));

        juce::var frame;

        const auto found = readEventUntil (desktop.socket, [&] (const juce::var& candidate)
        {
            return isEvent (candidate, "pad")
                   && intProperty (property (candidate, "event"), "pad") == pad
                   && boolProperty (property (candidate, "event"), "pressed");
        }, 1000, frame);

        const double latency = elapsedMs (pressedAt);
        rig.worstPadLatencyMs = std::max (rig.worstPadLatencyMs, latency);

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

            check (intProperty (frame, "sequence") >= expectedEvents,
                   "the pad frame carries a fresh sequence number");
        }

        juce::var tabletFrame;
        const auto tabletFound = readEventUntil (tablet.socket, [&] (const juce::var& candidate)
        {
            return isEvent (candidate, "pad")
                   && intProperty (property (candidate, "event"), "pad") == pad
                   && boolProperty (property (candidate, "event"), "pressed");
        }, 1000, tabletFrame);

        const double tabletLatency = elapsedMs (pressedAt);
        rig.worstPadLatencyMs = std::max (rig.worstPadLatencyMs, tabletLatency);

        check (tabletFound && tabletLatency <= kLatencyBudgetMs,
               "the same pad press reached the tablet socket in "
                   + std::to_string (tabletLatency) + " ms");

        check (rig.padsSeen.load() >= expectedEvents,
               "the driver delivered the pad event to the broadcaster");

        rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (
            1, note, static_cast<juce::uint8> (0)));

        juce::var released;
        const auto releaseFound = readEventUntil (desktop.socket, [&] (const juce::var& candidate)
        {
            return isEvent (candidate, "pad")
                   && intProperty (property (candidate, "event"), "pad") == pad
                   && ! boolProperty (property (candidate, "event"), "pressed");
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
}

} // namespace ApiTest::StateBroadcast
