// ihub-launchkey-protocol-test: acceptance check for T040.
//
// Pure logic only: no MIDI port is opened and no hardware is needed. The three
// acceptance checks are:
//
//   1. MIDI notes 36..51 map to pad indices 0..15, and back.
//   2. Every palette colour has its own distinct velocity byte.
//   3. The session-mode SysEx frame has the length and checksum recorded in
//      docs/launchkey-mini-mk3.md.

#include "controller/LaunchkeyProtocol.h"

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

std::string hexFrame (const std::vector<std::uint8_t>& bytes)
{
    std::string result;

    for (const auto byte : bytes)
        result += (result.empty() ? "" : " ") + hexByte (byte);

    return result;
}

void testPadMap()
{
    std::cout << "\n-- note 36..51 maps to pads 0..15 --" << std::endl;

    bool forward = true;
    bool reverse = true;

    for (int index = 0; index < Launchkey::padCount; ++index)
    {
        const int expectedNote = 36 + index;
        forward = forward && Launchkey::padNoteForIndex (index) == expectedNote;

        const auto mapped = Launchkey::padIndexForNote (expectedNote);
        reverse = reverse && mapped.has_value() && *mapped == index;
    }

    check (forward, "padNoteForIndex: pad 0..15 -> note 36..51");
    check (reverse, "padIndexForNote: note 36..51 -> pad 0..15");

    check (Launchkey::padNoteForIndex (-1) == -1
               && Launchkey::padNoteForIndex (16) == -1,
           "an out-of-range pad index has no note");

    check (! Launchkey::padIndexForNote (35).has_value()
               && ! Launchkey::padIndexForNote (52).has_value(),
           "notes outside 36..51 are not pads");

    check (Launchkey::isPadNote (36) && Launchkey::isPadNote (51)
               && ! Launchkey::isPadNote (35) && ! Launchkey::isPadNote (52),
           "isPadNote accepts exactly the 36..51 range");

    check (Launchkey::sessionLedNoteForIndex (0) == 112
               && Launchkey::sessionLedNoteForIndex (7) == 119
               && Launchkey::sessionLedNoteForIndex (8) == 96
               && Launchkey::sessionLedNoteForIndex (15) == 103,
           "vendor session LED notes: bottom row 112..119, top row 96..103");
}

void testPalette()
{
    std::cout << "\n-- palette colours have distinct velocity bytes --" << std::endl;

    bool inRange = true;
    bool distinct = true;
    std::array<bool, Launchkey::paletteSize> seen {};

    for (const auto colour : Launchkey::palette)
    {
        const auto velocity = Launchkey::velocityFor (colour);

        inRange = inRange && velocity < Launchkey::paletteSize;

        if (seen[velocity])
            distinct = false;

        seen[velocity] = true;
    }

    check (inRange, "every palette velocity is a 7-bit value (0..127)");
    check (distinct, "no two palette colours share a velocity byte");
    check (Launchkey::paletteVelocitiesAreDistinct(),
           "the compile-time uniqueness check agrees with the runtime scan");

    check (Launchkey::velocityFor (Launchkey::Colour::Off) == 0x00,
           "off is velocity 0x00");
    check (Launchkey::velocityFor (Launchkey::Colour::Red) == 0x05,
           "red is velocity 0x05 (documented example)");
    check (Launchkey::velocityFor (Launchkey::Colour::Green) == 0x13,
           "green is velocity 0x13 (documented example)");
    check (Launchkey::velocityFor (Launchkey::Colour::Blue) == 0x2D,
           "blue is velocity 0x2D (documented example)");

    const auto staticMessage = Launchkey::padColourMessage (0, Launchkey::Colour::Red);
    const auto flashingMessage = Launchkey::padColourMessage (15, Launchkey::Colour::Green,
                                                              Launchkey::LedMode::Flashing);
    const auto pulsingMessage = Launchkey::padColourMessage (8, Launchkey::Colour::Blue,
                                                             Launchkey::LedMode::Pulsing);
    const auto invalidMessage = Launchkey::padColourMessage (16, Launchkey::Colour::Red);

    check (staticMessage == std::array<std::uint8_t, 3> { 0x90, 112, 0x05 },
           "static red on pad 0 is 90 70 05");
    check (flashingMessage == std::array<std::uint8_t, 3> { 0x91, 103, 0x13 },
           "flashing green on pad 15 is 91 67 13");
    check (pulsingMessage == std::array<std::uint8_t, 3> { 0x92, 96, 0x2D },
           "pulsing blue on pad 8 is 92 60 2D");
    check (invalidMessage == std::array<std::uint8_t, 3> { 0, 0, 0 },
           "an out-of-range pad produces no LED message");
}

void testEncodersAndKeys()
{
    std::cout << "\n-- encoder CC map and key range --" << std::endl;

    bool forward = true;
    bool reverse = true;

    for (int index = 0; index < Launchkey::encoderCount; ++index)
    {
        forward = forward && Launchkey::encoderCCForIndex (index) == 21 + index;

        const auto mapped = Launchkey::encoderIndexForCC (21 + index);
        reverse = reverse && mapped.has_value() && *mapped == index;
    }

    check (forward, "encoders 0..7 map to CC 21..28");
    check (reverse, "CC 21..28 map back to encoders 0..7");
    check (! Launchkey::encoderIndexForCC (20).has_value()
               && ! Launchkey::encoderIndexForCC (29).has_value(),
           "CCs outside 21..28 are not encoders");
    check (Launchkey::encoderStatus == 0xBF && Launchkey::encoderMidiChannel == 16,
           "encoder messages arrive as BF (channel 16)");

    check (Launchkey::keyCount == 25 && Launchkey::keyNoteFirst == 48
               && Launchkey::keyNoteLast == 72,
           "25 keys span MIDI 48..72 (C2..C4, C3 = 60)");
    check (Launchkey::isKeyNote (48) && Launchkey::isKeyNote (72)
               && ! Launchkey::isKeyNote (47) && ! Launchkey::isKeyNote (73),
           "isKeyNote accepts exactly the 25-key range");
}

void testSysEx()
{
    std::cout << "\n-- documented SysEx frame length and checksum --" << std::endl;

    const auto frame = Launchkey::sessionModeFrame();

    const std::vector<std::uint8_t> documented {
        0xF0, 0x00, 0x20, 0x29, 0x02, 0x0F, 0x0C, 0x01, 0x0D, 0xF7
    };

    std::cout << "frame: " << hexFrame (frame.bytes) << " (" << frame.size()
              << " bytes, checksum " << hexByte (frame.bytes.at (8)) << ")"
              << std::endl;

    check (frame.size() == documented.size() && frame.size() == 10,
           "session-mode frame is 10 bytes long");
    check (frame.bytes == documented,
           "session-mode frame matches the documented example byte for byte");
    check (frame.bytes[8] == 0x0D,
           "checksum is 0x0D (XOR of command 0x0C and payload 0x01)");
    check (Launchkey::checksumOf ({ frame.bytes.data() + 6, 2 }) == 0x0D,
           "checksumOf recomputes the documented checksum");
    check (frame.isValid(), "the documented frame validates");
    check (Launchkey::isSessionModeFrame (frame.bytes),
           "isSessionModeFrame recognises the documented frame");

    auto badChecksum = frame.bytes;
    badChecksum[8] ^= 0x01;
    Launchkey::SysExFrame badChecksumFrame { badChecksum, Launchkey::Checksum::Xor7 };
    check (! badChecksumFrame.isValid(), "a corrupted checksum is rejected");

    auto badLength = frame.bytes;
    badLength.pop_back();
    badLength.pop_back();
    badLength.push_back (0xF7);
    Launchkey::SysExFrame badLengthFrame { badLength, Launchkey::Checksum::Xor7 };
    check (! badLengthFrame.isValid(), "a truncated frame is rejected");

    Launchkey::SysExFrame wrongHeader { std::vector<std::uint8_t> {
        0xF0, 0x00, 0x20, 0x29, 0x02, 0x0E, 0x0C, 0x01, 0x0D, 0xF7 },
        Launchkey::Checksum::Xor7 };
    check (! wrongHeader.isValid(), "a frame for a different product is rejected");

    constexpr std::array<std::uint8_t, 1> highBitPayload { 0x80 };
    const auto invalidPayload = Launchkey::makeSysExFrame (
        Launchkey::SysExCommand::VelocityCurve, highBitPayload);
    check (invalidPayload.size() == 0 && ! invalidPayload.isValid(),
           "a non-7-bit payload is rejected at build time");

    const auto clearDisplay = Launchkey::makeSysExFrame (
        Launchkey::SysExCommand::ClearDisplay);
    check (clearDisplay.bytes == std::vector<std::uint8_t> {
               0xF0, 0x00, 0x20, 0x29, 0x02, 0x0F, 0x06, 0xF7 },
           "a documented command keeps the vendor frame (no added checksum)");
    check (clearDisplay.isValid(), "the generated vendor frame validates");

    constexpr std::array<std::uint8_t, 2> curve { 0x01, 0x02 };
    const auto checksummedCurve = Launchkey::makeSysExFrame (
        Launchkey::SysExCommand::VelocityCurve, curve, Launchkey::Checksum::Xor7);
    check (checksummedCurve.isValid() && checksummedCurve.bytes.back() == 0xF7
               && checksummedCurve.bytes[checksummedCurve.size() - 2] == 0x01,
           "the opt-in checksum is XOR(command, payload) = 0x01");

    check (Launchkey::dawModeOn == Launchkey::ChannelMessage { 0x9F, 0x0C, 0x7F },
           "DAW mode on is 9F 0C 7F");
    check (Launchkey::sessionPadLayout == Launchkey::ChannelMessage { 0xBF, 0x03, 0x02 },
           "session layout is BF 03 02");
    check (Launchkey::dawModeOff == Launchkey::ChannelMessage { 0x9F, 0x0C, 0x00 },
           "DAW mode off is 9F 0C 00");
}

} // namespace

int main()
{
    std::cout << "Launchkey Mini MK3 protocol (T040)" << std::endl;

    testPadMap();
    testPalette();
    testEncodersAndKeys();
    testSysEx();

    std::cout << "\n" << (failures == 0 ? "PASS" : "FAIL") << ": "
              << failures << " failure(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
