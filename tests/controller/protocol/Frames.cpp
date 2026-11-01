#include "Frames.h"

#include "../TestSupport.h"

#include "Hex.h"
#include "controller/LaunchpadProtocol.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ProtocolTest
{

using ControllerTest::check;

void testFrames()
{
    const auto programmer = Launchpad::programmerModeFrame (true);
    const std::vector<std::uint8_t> expectedProgrammer {
        0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7
    };

    check (programmer.isValid(), "the programmer-mode frame is valid");
    check (programmer.bytes == expectedProgrammer,
           "programmer mode is F0 00 20 29 02 0D 0E 01 F7, got "
               + hexBytes (programmer.bytes));
    check (Launchpad::isProgrammerModeFrame (programmer.data()),
           "the programmer-mode frame is recognised");

    const auto live = Launchpad::programmerModeFrame (false);

    check (live.bytes == std::vector<std::uint8_t> { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D,
                                                     0x0E, 0x00, 0xF7 },
           "Live mode is 0E 00");

    const auto layout = Launchpad::selectLayoutFrame (Launchpad::Layout::Programmer);

    check (layout.bytes == std::vector<std::uint8_t> { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D,
                                                       0x00, 0x7F, 0xF7 },
           "layout 0x7F is F0 00 20 29 02 0D 00 7F F7");

    const auto feedback = Launchpad::ledFeedbackFrame (false, true);

    check (feedback.bytes == std::vector<std::uint8_t> { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D,
                                                         0x0A, 0x00, 0x01, 0xF7 },
           "external LED feedback is 0A 00 01");

    const auto awake = Launchpad::ledSleepFrame (true);

    check (awake.bytes == std::vector<std::uint8_t> { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D,
                                                      0x09, 0x01, 0xF7 },
           "wake from LED sleep is 09 01");

    const std::array<std::uint8_t, 1> outOfRange { 0x80 };
    const auto rejected = Launchpad::makeSysExFrame (Launchpad::SysExCommand::SelectLayout,
                                                     outOfRange);

    check (rejected.bytes.empty() && ! rejected.isValid(),
           "a non-7-bit payload is rejected instead of corrupting the message");

    Launchpad::SysExFrame broken = programmer;
    broken.bytes[2] = 0x21;
    check (! broken.isValid(), "a wrong manufacturer byte is rejected");

    Launchpad::SysExFrame noEnd = programmer;
    noEnd.bytes.back() = 0x00;
    check (! noEnd.isValid(), "a missing F7 is rejected");
}

}
