#include "Grid.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace DriverTest
{

using ControllerTest::check;

void runGridChecks (Rig& rig)
{
    rig.armedSlots.clear();
    rig.activePad = -1;

    bool allPadsArm = true;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const int note = Launchpad::padNoteForIndex (pad);

        rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (
            1, note, std::uint8_t (100)));

        const bool armed = rig.mapping.getArmedSlot() == pad
                           && rig.mapping.getActivePad() == pad;

        if (! armed)
        {
            check (false, "scripted pad " + std::to_string (pad)
                              + " (note " + std::to_string (note) + ") arms slot "
                              + std::to_string (pad));
            allPadsArm = false;
        }

        rig.driver.injectMidiMessage (juce::MidiMessage::noteOn (
            1, note, std::uint8_t (0)));
    }

    check (allPadsArm, "all 64 scripted pads arm their own slot");

    std::vector<int> expectedSlots;
    expectedSlots.reserve (64);

    for (int pad = 0; pad < 64; ++pad)
        expectedSlots.push_back (pad);

    check (rig.armedSlots == expectedSlots,
           "the driver produced the armed slot sequence 0..63");
}

}
