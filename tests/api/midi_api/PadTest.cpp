#include "PadTest.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"
#include "Fixtures.h"

#include "api/MidiBridge.h"
#include "controller/LaunchpadDriver.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ApiTest::MidiApi
{

void runPadTestStream()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    Controller::LaunchpadDriver driver;
    std::vector<juce::MidiMessage> sent;
    std::mutex sentMutex;
    driver.setMessageSink ([&sent, &sentMutex] (const juce::MidiMessage& message)
    {
        const std::lock_guard<std::mutex> lock (sentMutex);
        sent.push_back (message);
    });

    Api::MidiBridge bridge;
    bool restored = false;
    bridge.setRestoreHandler ([&restored] { restored = true; });
    bridge.setLaunchpad (&driver);

    std::string error;
    check (bridge.runPadTest (error), "the pad test runs through a scripted LED sink");
    check (error.empty(), "the scripted pad test has no error");

    int lit = 0;
    bool notesInRange = true;
    std::vector<juce::MidiMessage> firstFrame;

    {
        const std::lock_guard<std::mutex> lock (sentMutex);
        firstFrame = sent;
    }

    for (const auto& message : firstFrame)
    {
        if (! message.isNoteOn() || message.getVelocity() <= 0)
            continue;

        ++lit;
        const int note = message.getNoteNumber();

        if (note < 11 || note > 88)
            notesInRange = false;
    }

    check (lit == 64, "the first frame lights all 64 pads with Note On");
    check (notesInRange, "the lit notes are the Launchpad grid notes 11..88");

    const auto active = bridge.snapshot();

    check (active.padTest.active, "the snapshot reports the running test");
    check (active.padTest.lit == 64, "the snapshot reports 64 lit pads");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (3);

    while (bridge.isPadTestActive() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (20));

    check (! bridge.isPadTestActive(), "the pad test stops by itself");
    check (restored, "the pad test restores the grid through its callback");

    const auto after = bridge.snapshot();
    check (! after.padTest.active && after.padTest.lit == 0,
           "the finished test reports no lit pads");

    bool clears = false;

    {
        const std::lock_guard<std::mutex> lock (sentMutex);
        clears = std::any_of (sent.begin(), sent.end(),
                              [] (const juce::MidiMessage& message)
                              {
                                  return (message.isNoteOn() || message.isNoteOff())
                                      && message.getNoteNumber() >= 11
                                      && message.getNoteNumber() <= 88
                                      && message.getVelocity() == 0;
                              });
    }

    check (clears, "the test clears the pads when it ends");

    bridge.setLaunchpad (nullptr);
    check (! bridge.runPadTest (error), "the pad test refuses without a Launchpad");
    check (! error.empty(), "the refusal explains why");
}

} // namespace ApiTest::MidiApi
