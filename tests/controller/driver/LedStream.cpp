#include "LedStream.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace DriverTest
{

using ControllerTest::check;

void runLedStreamChecks (Rig& rig)
{
    rig.driver.injectMidiMessage (juce::MidiMessage::controllerEvent (1, 91, 127));
    check (rig.mapping.getActiveBank() == 0, "CC 91 selects macro page 0");

    rig.driver.injectMidiMessage (juce::MidiMessage::controllerEvent (1, 89, 127));
    check (rig.mapping.getActiveScene() == 0, "CC 89 selects scene 0");

    rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 81, std::uint8_t (100)));
    rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 81, std::uint8_t (0)));

    rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 11, std::uint8_t (100)));
    rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (1, 11, std::uint8_t (0)));

    rig.driver.setPadState (63, Controller::PadState::error);
    rig.driver.setPadState (63, Controller::PadState::idle);

    check (rig.armedSlots == std::vector<int> { 56, 0 },
           "the scripted pads armed slots 56 then 0");

    std::vector<Bytes> expected;

    for (int index = 0; index < Launchpad::topRowCount; ++index)
        expected.push_back (buttonLed (Launchpad::topRowCCForIndex (index),
                                       index == 0 ? Controller::PadState::active
                                                  : Controller::PadState::idle));

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

    check (rig.sent == expected,
           "the LED stream matches idle/warming/active/error exactly");

    if (rig.sent != expected)
        std::cout << "  expected: " << hex (expected) << "\n"
                  << "  actual  : " << hex (rig.sent) << std::endl;

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
}

}
