// ihub-launchpad-driver-test: the T116 scripted driver acceptance check.
//
// No MIDI port is opened. Incoming notes/CCs are injected through the same
// decoder the MIDI thread uses, and outgoing LED messages are captured by a
// message sink. The test proves:
//
//   1. The programmer-mode echo is recognised.
//   2. Top-row CCs select macro pages and light the matching button.
//   3. Right-column CCs select scenes and light the matching button.
//   4. Pad presses arm the slot from set.yaml and produce an exact
//      idle/warming/active/error LED stream.
//   5. A scripted press of every one of the 64 pads arms slots 0..63.

#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <juce_events/juce_events.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace
{
int failures = 0;

using Bytes = std::vector<std::uint8_t>;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string hex (const Bytes& bytes)
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

std::string hex (const std::vector<Bytes>& messages)
{
    std::string result;

    for (const auto& message : messages)
        result += (result.empty() ? "" : " | ") + hex (message);

    return result;
}

Bytes padLed (int pad, Controller::PadState state)
{
    const auto bytes = Launchpad::padColourMessage (
        pad, Controller::launchpadColourForState (state));
    return Bytes (bytes.begin(), bytes.end());
}

Bytes buttonLed (int cc, Controller::PadState state)
{
    const auto bytes = Launchpad::buttonColourMessage (
        cc, Controller::launchpadColourForState (state));
    return Bytes (bytes.begin(), bytes.end());
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    Controller::Mapping mapping;
    Controller::MappingError error;

    check (mapping.load (IHUB_SET_YAML, &error),
           std::string ("shared/set.yaml loads")
               + (error.isEmpty() ? std::string() : " (" + error.message + ")"));

    Controller::LaunchpadDriver driver;

    std::vector<Bytes> sent;
    driver.setMessageSink ([&] (const juce::MidiMessage& message)
    {
        sent.emplace_back (message.getRawData(),
                           message.getRawData() + message.getRawDataSize());
    });

    std::vector<int> armedSlots;
    int activePad = -1;

    Controller::Mapping::Listeners mappingListeners;

    mappingListeners.armSlot = [&] (int slot) { armedSlots.push_back (slot); };

    // The app's LED policy: the previous pad goes idle, the new one warms for
    // a moment and then becomes active. Both transitions are in the stream.
    mappingListeners.padEvent = [&] (int pad,
                                     const Controller::PadBinding*,
                                     bool pressed)
    {
        if (! pressed)
            return;

        if (activePad >= 0)
            driver.setPadState (activePad, Controller::PadState::idle);

        driver.setPadState (pad, Controller::PadState::warming);
        driver.setPadState (pad, Controller::PadState::active);
        activePad = pad;
    };

    mappingListeners.bankEvent = [&] (int bank, const Controller::MacroBankSpec*)
    {
        for (int index = 0; index < Launchpad::topRowCount; ++index)
            driver.setButtonState (Launchpad::topRowCCForIndex (index),
                                   index == bank ? Controller::PadState::active
                                                 : Controller::PadState::idle);
    };

    mappingListeners.sceneEvent = [&] (int scene, const Controller::SceneSpec*)
    {
        for (int index = 0; index < Launchpad::rightColumnCount; ++index)
            driver.setButtonState (Launchpad::rightColumnCCForIndex (index),
                                   index == scene ? Controller::PadState::active
                                                  : Controller::PadState::idle);
    };

    mapping.setListeners (std::move (mappingListeners));

    Controller::LaunchpadDriver::DriverListeners driverListeners;

    driverListeners.padEvent = [&] (int pad, bool pressed)
    {
        mapping.handlePad (pad, pressed);
    };

    driverListeners.buttonEvent = [&] (int cc, bool pressed)
    {
        mapping.handleButton (cc, pressed);
    };

    driver.setListeners (std::move (driverListeners));

    //==========================================================================
    // Handshake echo is recognised through the same decoder.
    //==========================================================================
    const auto programmer = Launchpad::programmerModeFrame (true);
    driver.injectMidiMessage (juce::MidiMessage (programmer.bytes.data(),
                                                 static_cast<int> (programmer.bytes.size())));

    check (driver.hasReceivedHandshakeEcho(),
           "an injected programmer-mode echo is recognised");

    //==========================================================================
    // Scripted event sequence
    //==========================================================================
    sent.clear();

    driver.injectMidiMessage (juce::MidiMessage::controllerEvent (1, 91, 127));
    check (mapping.getActiveBank() == 0, "CC 91 selects macro page 0");

    driver.injectMidiMessage (juce::MidiMessage::controllerEvent (1, 89, 127));
    check (mapping.getActiveScene() == 0, "CC 89 selects scene 0");

    driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 81, std::uint8_t (100)));
    driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 81, std::uint8_t (0)));

    driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 11, std::uint8_t (100)));
    driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 11, std::uint8_t (0)));

    driver.setPadState (63, Controller::PadState::error);
    driver.setPadState (63, Controller::PadState::idle);

    check (armedSlots == std::vector<int> { 56, 0 },
           "the scripted pads armed slots 56 then 0");

    std::vector<Bytes> expected;

    // Top row: all eight buttons rewritten, bank 0 active.
    for (int index = 0; index < Launchpad::topRowCount; ++index)
        expected.push_back (buttonLed (Launchpad::topRowCCForIndex (index),
                                       index == 0 ? Controller::PadState::active
                                                  : Controller::PadState::idle));

    // Right column: all eight buttons rewritten, scene 0 active.
    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        expected.push_back (buttonLed (Launchpad::rightColumnCCForIndex (index),
                                       index == 0 ? Controller::PadState::active
                                                  : Controller::PadState::idle));

    expected.push_back (padLed (56, Controller::PadState::warming));
    expected.push_back (padLed (56, Controller::PadState::active));
    expected.push_back (padLed (56, Controller::PadState::idle));
    expected.push_back (padLed (0, Controller::PadState::warming));
    expected.push_back (padLed (0, Controller::PadState::active));
    expected.push_back (padLed (63, Controller::PadState::error));
    expected.push_back (padLed (63, Controller::PadState::idle));

    check (sent == expected,
           "the LED stream matches idle/warming/active/error exactly");

    if (sent != expected)
        std::cout << "  expected: " << hex (expected) << "\n"
                  << "  actual  : " << hex (sent) << std::endl;

    check (hex (padLed (0, Controller::PadState::active)) == "90 0B 15",
           "pad 0 active is green (90 0B 15)");
    check (hex (padLed (0, Controller::PadState::warming)) == "90 0B 0D",
           "pad 0 warming is yellow (90 0B 0D)");
    check (hex (padLed (0, Controller::PadState::error)) == "90 0B 05",
           "pad 0 error is red (90 0B 05)");
    check (hex (padLed (0, Controller::PadState::idle)) == "90 0B 00",
           "pad 0 idle is off (90 0B 00)");
    check (hex (buttonLed (91, Controller::PadState::active)) == "B0 5B 15",
           "top-row button 0 active is green (B0 5B 15)");
    check (hex (buttonLed (89, Controller::PadState::active)) == "B0 59 15",
           "right-column button 0 active is green (B0 59 15)");

    //==========================================================================
    // Every pad through the decoder
    //==========================================================================
    armedSlots.clear();
    activePad = -1;

    bool allPadsArm = true;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const int note = Launchpad::padNoteForIndex (pad);

        driver.injectMidiMessage (juce::MidiMessage::noteOn (
            1, note, std::uint8_t (100)));

        const bool armed = mapping.getArmedSlot() == pad
                           && mapping.getActivePad() == pad;

        if (! armed)
        {
            check (false, "scripted pad " + std::to_string (pad)
                              + " (note " + std::to_string (note) + ") arms slot "
                              + std::to_string (pad));
            allPadsArm = false;
        }

        driver.injectMidiMessage (juce::MidiMessage::noteOn (
            1, note, std::uint8_t (0)));
    }

    check (allPadsArm, "all 64 scripted pads arm their own slot");
    check (armedSlots == [] {
               std::vector<int> slots;
               slots.reserve (64);

               for (int pad = 0; pad < 64; ++pad)
                   slots.push_back (pad);

               return slots;
           }(),
           "the driver produced the armed slot sequence 0..63");

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
