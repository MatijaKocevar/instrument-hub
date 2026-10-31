// ihub-launchkey-driver-test: the T042 acceptance check.
//
// It creates the ALSA-sequencer `Launchkey Mini MK3 SIM` from T041, lets the
// real LaunchkeyDriver find it through juce::MidiInput/MidiOutput, and then:
//
//   1. checks the connect handshake byte for byte against the fixture
//      tests/midi/fixtures/session-handshake.json;
//   2. replays the fixture's pad 0 press and checks the mapping callback sees
//      pad 0 and arms slot 0;
//   3. checks the driver wrote the pad 0 LED with the `active` colour;
//   4. checks key notes and encoder CCs are forwarded, and that pad notes are
//      not also reported as keys.

#include "controller/LaunchkeyDriver.h"

#include "LaunchkeySim.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef IHUB_MIDI_FIXTURE_DIR
 #define IHUB_MIDI_FIXTURE_DIR "tests/midi/fixtures"
#endif

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string fixturePath (const std::string& name)
{
    return std::string (IHUB_MIDI_FIXTURE_DIR) + "/" + name;
}

std::string hex (const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill ('0');

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
            out << ' ';

        out << std::setw (2) << static_cast<unsigned int> (bytes[i]);
    }

    return out.str();
}

bool waitFor (const std::function<bool()>& predicate, int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);

    while (! predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;

        std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }

    return true;
}

bool bytesEqual (const std::vector<std::uint8_t>& actual,
                 std::initializer_list<std::uint8_t> expected)
{
    return actual.size() == expected.size()
           && std::equal (actual.begin(), actual.end(), expected.begin());
}

bool isNoteOn (const LaunchkeySim::RecordedMessage& message,
               int channel, int note, int velocity)
{
    return message.type == "noteon" && message.channel == channel
           && message.data1 == note && message.data2 == velocity;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    try
    {
        LaunchkeySim::Simulator simulator;
        simulator.open();

        std::cout << "driver: simulator port " << simulator.clientId() << ":"
                  << simulator.portId() << " '" << LaunchkeySim::kPortName << "'"
                  << std::endl;

        // --- Detection ---------------------------------------------------
        const auto selection = Controller::LaunchkeyDriver::findDevice();
        check (selection.has_value(), "Launchkey Mini MK3 detected by name");

        if (! selection.has_value())
        {
            std::cout << "FAIL: no controller visible to JUCE; available inputs:";

            for (const auto& device : juce::MidiInput::getAvailableDevices())
                std::cout << " ['" << device.name << "']";

            std::cout << std::endl;
            return 1;
        }

        std::cout << "driver: input='" << selection->inputName << "' id="
                  << selection->inputIdentifier << " output='"
                  << selection->outputName << "' id="
                  << selection->outputIdentifier << std::endl;

        // --- Connect and handshake ---------------------------------------
        Controller::LaunchkeyDriver driver;
        const bool connected = driver.connect();

        check (connected, "connect() opens both directions"
                          + (connected ? std::string()
                                       : " (" + driver.getLastError().toStdString() + ")"));

        if (! connected)
            return 1;

        check (driver.isConnected(), "driver reports connected");

        const auto handshakeFixture = LaunchkeySim::loadFixture (
            fixturePath ("session-handshake.json"));

        simulator.drain (300);
        const auto& handshake = simulator.recorded();

        check (handshake.size() == 3,
               "connect emitted exactly the 3 handshake messages (got "
               + std::to_string (handshake.size()) + ")");

        if (handshake.size() >= 3)
        {
            check (handshake[0].type == "sysex"
                   && handshake[0].bytes == handshakeFixture.events[0].bytes,
                   "handshake SysEx matches fixture: " + hex (handshake[0].bytes));

            check (isNoteOn (handshake[1], 16, 12, 127)
                   && bytesEqual (handshake[1].bytes, { 0x9F, 0x0C, 0x7F }),
                   "handshake DAW-mode note matches fixture: "
                   + hex (handshake[1].bytes));

            check (handshake[2].type == "cc" && handshake[2].channel == 16
                   && handshake[2].data1 == 3 && handshake[2].data2 == 2
                   && bytesEqual (handshake[2].bytes, { 0xBF, 0x03, 0x02 }),
                   "handshake session-layout CC matches fixture: "
                   + hex (handshake[2].bytes));
        }

        // --- Mapping callbacks -------------------------------------------
        int armedSlot = -1;
        int padDowns = 0;
        int padUps = 0;
        int lastPad = -1;
        int keyDowns = 0;
        int lastKey = -1;
        int lastEncoder = -1;
        int lastEncoderValue = -1;

        Controller::DriverListeners listeners;

        listeners.padEvent = [&] (int pad, bool pressed)
        {
            lastPad = pad;

            if (! pressed)
            {
                ++padUps;
                return;
            }

            ++padDowns;

            if (pad == 0)
            {
                armedSlot = 0;
                driver.setPadState (0, Controller::PadState::active);
            }
        };

        listeners.keyEvent = [&] (int note, float, bool pressed)
        {
            if (pressed)
            {
                ++keyDowns;
                lastKey = note;
            }
        };

        listeners.encoderEvent = [&] (int index, int value)
        {
            lastEncoder = index;
            lastEncoderValue = value;
        };

        driver.setListeners (listeners);

        // --- Pad 0 press -> arm slot 0 -> active LED ----------------------
        simulator.clearRecording();
        simulator.replay (handshakeFixture);

        check (waitFor ([&] { return armedSlot == 0; }, 2000),
               "pad 0 press arms slot 0");

        simulator.drain (300);

        check (padDowns == 1, "mapping saw exactly one pad press (got "
                              + std::to_string (padDowns) + ")");
        check (padUps == 1, "mapping saw exactly one pad release (got "
                            + std::to_string (padUps) + ")");
        check (lastPad == 0, "mapping saw pad index 0 (got "
                             + std::to_string (lastPad) + ")");
        check (driver.hasReceivedHandshakeEcho(),
               "driver recognised the session-mode SysEx echo");

        const auto activeColour = Controller::colourForState (
            Controller::PadState::active);
        const int ledNote = Launchkey::sessionLedNoteForIndex (0);

        bool foundActiveLed = false;

        for (const auto& message : simulator.recorded())
        {
            if (isNoteOn (message, 1, ledNote,
                          static_cast<int> (Launchkey::velocityFor (activeColour))))
                foundActiveLed = true;
        }

        check (activeColour == Launchkey::Colour::Green,
               "active state colour is green");
        check (foundActiveLed,
               "LED for pad 0 emitted with the active colour (note "
               + std::to_string (ledNote) + ", velocity "
               + std::to_string (static_cast<int> (Launchkey::velocityFor (activeColour)))
               + ")");

        // --- Key notes and encoders --------------------------------------
        LaunchkeySim::Fixture keyFixture;
        keyFixture.events.push_back ({ 0, "noteon", 1, 60, 100, 0, 0, {} });
        keyFixture.events.push_back ({ 20, "noteoff", 1, 60, 0, 0, 0, {} });
        keyFixture.events.push_back ({ 40, "cc", 16, 0, 0, 21, 65, {} });

        simulator.clearRecording();
        simulator.replay (keyFixture);

        check (waitFor ([&] { return keyDowns == 1 && lastEncoder == 0; }, 2000),
               "key note and encoder CC reach the mapping");

        simulator.drain (150);

        check (keyDowns == 1 && lastKey == 60,
               "key note 60 forwarded to the armed slot");
        check (lastEncoder == 0 && lastEncoderValue == 65,
               "encoder CC 21 decodes to encoder 0 value 65");
        check (padDowns == 1,
               "key note 60 was not mistaken for a pad event");

        // --- State colours -----------------------------------------------
        const auto idleColour = Controller::colourForState (Controller::PadState::idle);
        const auto warmingColour = Controller::colourForState (Controller::PadState::warming);
        const auto errorColour = Controller::colourForState (Controller::PadState::error);

        check (idleColour != warmingColour && warmingColour != activeColour
               && activeColour != errorColour && idleColour != errorColour,
               "idle/warming/active/error have distinct colours");

        driver.setPadState (1, Controller::PadState::error);
        simulator.drain (150);

        bool foundErrorLed = false;

        for (const auto& message : simulator.recorded())
        {
            if (isNoteOn (message, 1, Launchkey::sessionLedNoteForIndex (1),
                          static_cast<int> (Launchkey::velocityFor (errorColour))))
                foundErrorLed = true;
        }

        check (foundErrorLed, "setPadState(error) writes the error-colour LED");

        driver.disconnect();
        check (! driver.isConnected(), "disconnect() closes the device");

        std::cout << (failures == 0 ? "PASS" : "FAIL")
                  << ": LaunchkeyDriver acceptance (" << failures
                  << " failure(s))" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
