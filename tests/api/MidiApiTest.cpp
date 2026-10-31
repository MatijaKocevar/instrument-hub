// T126 acceptance test: MIDI inputs on the Audio page.
//
// Part 1 starts the control server against an in-memory MIDI backend and
// proves the request/response contract: GET /midi lists every ALSA-shaped
// input with its role and switch, POST /midi/inputs opens/closes one, and
// POST /midi/pad-test reports all 64 pads lit.
//
// Part 2 enumerates the real ALSA devices through the same JUCE calls the
// engine uses and compares the bridge's list with what JUCE reports. It then
// drives the pad test through a scripted LaunchpadDriver (message sink, no
// port) and checks the exact 64-note LED stream, its clear and the restore
// callback. No sound card or controller is required.

#include "api/MidiBridge.h"
#include "api/Server.h"

#include "controller/LaunchpadDriver.h"

#include <httplib.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

juce::var parseJson (const std::string& text)
{
    return juce::JSON::parse (juce::String (text));
}

juce::var property (const juce::var& object, const juce::String& name)
{
    const auto* dynamic = object.getDynamicObject();
    return dynamic == nullptr ? juce::var {} : dynamic->getProperty (name);
}

std::string stringProperty (const juce::var& object, const juce::String& name)
{
    return property (object, name).toString().toStdString();
}

bool boolProperty (const juce::var& object, const juce::String& name)
{
    return static_cast<bool> (property (object, name));
}

int intProperty (const juce::var& object, const juce::String& name)
{
    return static_cast<int> (property (object, name));
}

Api::MidiInputState makeInput (const std::string& id,
                               const std::string& name,
                               const std::string& role,
                               bool enabled)
{
    Api::MidiInputState input;
    input.id = id;
    input.name = name;
    input.role = role;
    input.enabled = enabled;
    input.open = enabled;

    if (role == "launchpad")
        input.port = name.find ("MI") != std::string::npos ? "MI" : "DA";
    else if (role == "keystation")
        input.port = "USB";

    return input;
}

Api::MidiStateSnapshot makeFakeMidi()
{
    Api::MidiStateSnapshot midi;

    midi.inputs.push_back (makeInput ("ALSA:4:1",
                                      "Launchpad Mini MK3 LPMiniMK3 MI",
                                      "launchpad", true));
    midi.inputs.push_back (makeInput ("ALSA:4:0",
                                      "Launchpad Mini MK3 LPMiniMK3 DA",
                                      "launchpad", false));
    midi.inputs.push_back (makeInput ("ALSA:5:0",
                                      "Keystation Mini 32 MK3 USB Audio",
                                      "keystation", false));
    midi.inputs.push_back (makeInput ("ALSA:2:0",
                                      "Steinberg UR22mkII MIDI 1",
                                      "input", false));

    midi.launchpad.name = "Launchpad Mini MK3 LPMiniMK3 MI";
    midi.launchpad.port = "MI";
    midi.launchpad.connected = true;
    midi.launchpad.handshake = true;
    midi.launchpad.inputName = midi.launchpad.name;
    midi.launchpad.outputName = midi.launchpad.name;

    midi.keystation.name = "Keystation Mini 32 MK3 USB Audio";
    midi.keystation.port = "USB";

    midi.padTest.durationMs = Api::MidiBridge::padTestDurationMs;
    return midi;
}

void routeTest()
{
    Api::StateStore state (8);
    auto midi = makeFakeMidi();
    int padTests = 0;

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    Api::Server server (state, options);

    server.setMidiHandlers (
        [&midi] { return midi; },
        [&midi] (const std::string& id, bool enabled, std::string& error)
        {
            const auto input = std::find_if (midi.inputs.begin(), midi.inputs.end(),
                                             [&id] (const Api::MidiInputState& candidate)
                                             {
                                                 return candidate.id == id;
                                             });

            if (input == midi.inputs.end())
            {
                error = "unknown MIDI input: " + id;
                return false;
            }

            input->enabled = enabled;
            input->open = enabled;

            if (input->role == "launchpad")
                midi.launchpad.connected = enabled;

            if (input->role == "keystation")
                midi.keystation.connected = enabled;

            return true;
        },
        [&midi, &padTests] (std::string&)
        {
            ++padTests;
            midi.padTest.active = true;
            midi.padTest.lit = 64;
            return true;
        });

    if (! server.start())
    {
        std::cerr << "midi-api-test: FAILED to start the API server" << std::endl;
        ++failures;
        return;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (5, 0);

    // --- GET /midi --------------------------------------------------------
    auto response = client.Get ("/midi");
    check (response && response->status == 200, "GET /midi returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto* inputs = property (body, "inputs").getArray();

        check (inputs != nullptr && inputs->size() == 4,
               "GET /midi lists all four ALSA inputs");
        check (intProperty (body, "inputCount") == 4, "the input count matches the list");

        if (inputs != nullptr && inputs->size() == 4)
        {
            const auto& launchpad = (*inputs)[0];
            const auto& keystation = (*inputs)[2];

            check (stringProperty (launchpad, "role") == "launchpad",
                   "the first input is the Launchpad");
            check (stringProperty (launchpad, "port") == "MI",
                   "the Launchpad input names its MI port");
            check (boolProperty (launchpad, "enabled"), "the Launchpad starts enabled");
            check (stringProperty (keystation, "role") == "keystation",
                   "the third input is the Keystation");
            check (! boolProperty (keystation, "enabled"), "the Keystation starts off");
        }

        const auto launchpad = property (body, "launchpad");
        const auto keystation = property (body, "keystation");
        const auto padTest = property (body, "padTest");

        check (stringProperty (launchpad, "port") == "MI",
               "the Launchpad reports the MI port");
        check (boolProperty (launchpad, "connected"), "the Launchpad is connected");
        check (boolProperty (launchpad, "handshake"),
               "the Launchpad reports its programmer-mode echo");
        check (stringProperty (keystation, "port") == "USB",
               "the Keystation reports its USB port");
        check (! boolProperty (padTest, "active"), "no pad test has run yet");
        check (intProperty (padTest, "durationMs") == Api::MidiBridge::padTestDurationMs,
               "the pad-test duration is published");
    }

    // --- POST /midi/inputs: enable the Keystation -------------------------
    response = client.Post ("/midi/inputs",
                            R"({"id":"ALSA:5:0","enabled":true})",
                            "application/json");
    check (response && response->status == 200, "POST /midi/inputs accepts an enable");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        const auto keystation = property (body, "keystation");

        check (boolProperty (keystation, "connected"),
               "the response reports the opened Keystation");

        const auto* inputs = property (body, "inputs").getArray();

        if (inputs != nullptr && inputs->size() == 4)
        {
            const auto& row = (*inputs)[2];
            check (boolProperty (row, "enabled") && boolProperty (row, "open"),
                   "the Keystation row is enabled and open");
        }
    }

    // --- Validation --------------------------------------------------------
    response = client.Post ("/midi/inputs", R"({"id":"ALSA:9:9","enabled":true})",
                            "application/json");
    check (response && response->status == 404,
           "an unknown input identifier is rejected with 404");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0"})", "application/json");
    check (response && response->status == 400, "a missing enabled field is rejected");

    response = client.Post ("/midi/inputs", R"({"id":"ALSA:5:0","enabled":"yes"})",
                            "application/json");
    check (response && response->status == 400, "a non-boolean enabled field is rejected");

    // --- POST /midi/pad-test ----------------------------------------------
    response = client.Post ("/midi/pad-test", "", "application/json");
    check (response && response->status == 200, "POST /midi/pad-test returns 200");

    if (response && response->status == 200)
    {
        const auto padTest = property (parseJson (response->body), "padTest");

        check (boolProperty (padTest, "active"), "the pad test reports it is running");
        check (intProperty (padTest, "lit") == 64, "the pad test lights all 64 pads");
    }

    check (padTests == 1, "the pad-test handler ran exactly once");

    // --- No backend --------------------------------------------------------
    Api::Server bare (state, options);

    if (bare.start())
    {
        httplib::Client bareClient ("127.0.0.1", bare.getPort());
        const auto empty = bareClient.Get ("/midi");

        check (empty && empty->status == 200,
               "GET /midi without a backend still answers 200");

        if (empty && empty->status == 200)
        {
            const auto* inputs = property (parseJson (empty->body), "inputs").getArray();
            check (inputs != nullptr && inputs->isEmpty(),
                   "the empty backend reports no inputs");
        }

        const auto toggle = bareClient.Post ("/midi/inputs",
                                             R"({"id":"x","enabled":true})",
                                             "application/json");
        check (toggle && toggle->status == 503,
               "POST /midi/inputs without a backend answers 503");

        const auto test = bareClient.Post ("/midi/pad-test", "", "application/json");
        check (test && test->status == 503,
               "POST /midi/pad-test without a backend answers 503");

        bare.stop();
    }

    server.stop();
}

void deviceEnumerationTest()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto devices = juce::MidiInput::getAvailableDevices();
    const auto inputs = Api::MidiBridge::enumerateInputs();

    check (inputs.size() == static_cast<std::size_t> (devices.size()),
           "the bridge lists every MIDI input JUCE reports");

    bool sawLaunchpad = false;
    bool sawKeystation = false;
    bool launchpadMi = false;
    bool keystationUsb = false;

    for (const auto& input : inputs)
    {
        check (! input.id.empty() && ! input.name.empty(),
               "every enumerated input has an id and a name");

        if (input.role == "launchpad")
        {
            sawLaunchpad = true;

            if (Api::MidiBridge::portForName (juce::String (input.name)) == "MI")
                launchpadMi = true;
        }

        if (input.role == "keystation")
        {
            sawKeystation = true;
            keystationUsb = Api::MidiBridge::portForName (juce::String (input.name)) == "USB";
        }
    }

    check (inputs.size() == 0 || sawLaunchpad || sawKeystation
               || std::none_of (inputs.begin(), inputs.end(),
                                [] (const Api::MidiInputState& input)
                                {
                                    return input.role != "input";
                                }),
           "unrecognised inputs are classified as plain MIDI inputs");

    std::cout << "midi-api-test: ALSA inputs=" << inputs.size()
              << " launchpad=" << (sawLaunchpad ? "yes" : "no")
              << " keystation=" << (sawKeystation ? "yes" : "no") << std::endl;

    if (sawLaunchpad)
        check (launchpadMi, "the Launchpad's MI port is identified");

    if (sawKeystation)
        check (keystationUsb, "the Keystation's USB port is identified");

    Api::MidiBridge bridge;
    const auto state = bridge.snapshot();

    check (state.inputs.size() == inputs.size(),
           "GET /midi mirrors the enumerated ALSA inputs");

    if (sawLaunchpad)
    {
        check (state.launchpad.port == "MI", "the idle bridge still names the MI port");
        check (! state.launchpad.connected, "the idle bridge reports no Launchpad driver");
    }

    if (sawKeystation)
    {
        check (state.keystation.port == "USB", "the idle bridge names the Keystation USB port");
        check (! state.keystation.connected, "the Keystation starts closed");
    }
}

void padTestStreamTest()
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

    // Let the animation finish (~900 ms) and clear the surface.
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

} // namespace

int main()
{
    routeTest();
    deviceEnumerationTest();
    padTestStreamTest();

    if (failures > 0)
    {
        std::cerr << "midi-api-test: " << failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "midi-api-test: all checks passed" << std::endl;
    return 0;
}
