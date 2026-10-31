// ihub-launchpad-protocol-test: hardware-free checks for the Launchpad Mini
// MK3 protocol description.
//
//   1. Notes 11..88 map to pad indices 0..63 and back, row-major.
//   2. Programmer mode, layout and LED SysEx frames match the bytes observed
//      on the device.
//   3. LED Note On messages use the grid note, not a separate LED note.
//   4. Every palette colour has its own velocity byte.

#include "controller/LaunchpadProtocol.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string hexByte (std::uint8_t byte)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw (2)
           << std::setfill ('0') << static_cast<int> (byte);
    return stream.str();
}

std::string hexBytes (const std::vector<std::uint8_t>& bytes)
{
    std::string result;

    for (const auto byte : bytes)
        result += (result.empty() ? "" : " ") + hexByte (byte);

    return result;
}

std::string hexBytes (const std::array<std::uint8_t, 3>& bytes)
{
    return hexBytes (std::vector<std::uint8_t> (bytes.begin(), bytes.end()));
}

void testGrid()
{
    check (Launchpad::padCount == 64, "the grid has 64 pads");

    for (int index = 0; index < Launchpad::padCount; ++index)
    {
        const int note = Launchpad::padNoteForIndex (index);

        check (note >= Launchpad::padNoteFirst && note <= Launchpad::padNoteLast,
               "pad " + std::to_string (index) + " has a grid note ("
                   + std::to_string (note) + ")");
        check (Launchpad::padIndexForNote (note) == index,
               "note " + std::to_string (note) + " maps back to pad "
                   + std::to_string (index));
    }

    check (Launchpad::padNoteForIndex (0) == 11, "pad 0 is note 11 (bottom-left)");
    check (Launchpad::padNoteForIndex (7) == 18, "pad 7 is note 18 (bottom-right)");
    check (Launchpad::padNoteForIndex (56) == 81, "pad 56 is note 81 (top-left)");
    check (Launchpad::padNoteForIndex (63) == 88, "pad 63 is note 88 (top-right)");

    check (Launchpad::padNoteForIndex (-1) == -1 && Launchpad::padNoteForIndex (64) == -1,
           "out-of-range pad indices have no note");

    for (const int note : { 0, 10, 19, 20, 80, 89, 90, 99, 100, 127 })
        check (Launchpad::padIndexForNote (note) == -1 && ! Launchpad::isGridNote (note),
               "note " + std::to_string (note) + " is not a grid note");

    check (Launchpad::padRowForIndex (0) == 0 && Launchpad::padColumnForIndex (0) == 0,
           "pad 0 is row 0, column 0");
    check (Launchpad::padRowForIndex (63) == 7 && Launchpad::padColumnForIndex (63) == 7,
           "pad 63 is row 7, column 7");
}

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

void testLeds()
{
    check (Launchpad::paletteVelocitiesAreDistinct(),
           "every named palette colour has a distinct velocity");

    check (Launchpad::velocityFor (Launchpad::Colour::Off) == 0, "off is velocity 0");
    check (Launchpad::velocityFor (Launchpad::Colour::Red) == 5, "red is velocity 5");
    check (Launchpad::velocityFor (Launchpad::Colour::Green) == 21, "green is velocity 21");
    check (Launchpad::velocityFor (Launchpad::Colour::Blue) == 45, "blue is velocity 45");

    const auto bottomLeft = Launchpad::padColourMessage (0, Launchpad::Colour::Green);
    check (bottomLeft == std::array<std::uint8_t, 3> { 0x90, 0x0B, 0x15 },
           "pad 0 green is 90 0B 15, got " + hexBytes (bottomLeft));

    const auto topRight = Launchpad::padColourMessage (63, Launchpad::Colour::Green);
    check (topRight == std::array<std::uint8_t, 3> { 0x90, 0x58, 0x15 },
           "pad 63 green is 90 58 15, got " + hexBytes (topRight));

    const auto flashing = Launchpad::padColourMessage (0, Launchpad::Colour::Red,
                                                       Launchpad::LedMode::Flashing);
    check (flashing == std::array<std::uint8_t, 3> { 0x91, 0x0B, 0x05 },
           "flashing red uses status 0x91");

    const auto pulsing = Launchpad::padColourMessage (0, Launchpad::Colour::Red,
                                                      Launchpad::LedMode::Pulsing);
    check (pulsing == std::array<std::uint8_t, 3> { 0x92, 0x0B, 0x05 },
           "pulsing red uses status 0x92");

    const auto invalid = Launchpad::padColourMessage (64, Launchpad::Colour::Green);
    check (invalid == std::array<std::uint8_t, 3> { 0, 0, 0 },
           "an out-of-range pad produces no LED message");
}

void testButtons()
{
    check (Launchpad::topRowCCForIndex (0) == 91
               && Launchpad::topRowCCForIndex (7) == 98,
           "top row CCs are 91..98");
    check (Launchpad::topRowIndexForCC (91) == 0
               && Launchpad::topRowIndexForCC (98) == 7,
           "top row CCs map back to buttons");
    check (! Launchpad::topRowIndexForCC (90) && ! Launchpad::topRowIndexForCC (99),
           "notes and the logo are not top-row buttons");

    check (Launchpad::rightColumnCCForIndex (0) == 89
               && Launchpad::rightColumnCCForIndex (7) == 19,
           "right column CCs are 89..19 top to bottom");
    check (Launchpad::rightColumnIndexForCC (89) == 0
               && Launchpad::rightColumnIndexForCC (19) == 7,
           "right column CCs map back to buttons");
    check (! Launchpad::rightColumnIndexForCC (88) && ! Launchpad::rightColumnIndexForCC (18),
           "non-column CCs are rejected");

    check (Launchpad::logoCC == 99, "the logo LED is CC 99");
}

} // namespace

int main()
{
    testGrid();
    testFrames();
    testLeds();
    testButtons();

    if (failures != 0)
    {
        std::cout << failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "PASS: Launchpad Mini MK3 protocol" << std::endl;
    return 0;
}
