#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/**
    Hardware-free description of the Novation Launchpad Mini MK3.

    Everything here is pure data and pure functions: no MIDI device is opened
    and nothing is called from the audio thread. The driver and the tests share
    this one description of the protocol.

    Sources, and the parts verified against the physical device on 2026-10-31:

    - Novation, *Launchpad Mini [MK3] Programmer's Reference Manual*:
      SysEx header, programmer mode, layout selection, palette, LED messages.
    - On-hardware capture: pads report notes 11..88 on the `MI` port, presses
      arrive as Note On, releases as Note On velocity 0, and the `MI` port is
      the only one that accepts LED Note On messages.

    Pad index used by Instrument Hub is row-major from the bottom-left:
    index 0 is note 11, index 63 is note 88. The grid note is
    `10 * row + column` with row and column in 1..8.
*/
namespace Launchpad
{

//==============================================================================
// Device identity and the SysEx envelope
//==============================================================================

/** Novation EMS registered MIDI manufacturer ID. */
inline constexpr std::array<std::uint8_t, 3> manufacturerId { 0x00, 0x20, 0x29 };

/** Launchpad MK3 product family, as used in the SysEx header. */
inline constexpr std::uint8_t productFamily = 0x02;

/** Launchpad Mini MK3 model byte (Launchpad X is 0x0C, Pro MK3 is 0x0E). */
inline constexpr std::uint8_t productModel = 0x0D;

/** The name JUCE/ALSA reports for the device. */
inline constexpr const char* deviceName = "Launchpad Mini MK3";

/** `F0 00 20 29 02 0D`: every Launchpad Mini MK3 SysEx message starts here. */
inline constexpr std::array<std::uint8_t, 6> sysExHeader { 0xF0, 0x00, 0x20, 0x29,
                                                           0x02, 0x0D };

inline constexpr std::uint8_t sysExStart = 0xF0;
inline constexpr std::uint8_t sysExEnd   = 0xF7;

/** Command bytes used by this project. */
enum class SysExCommand : std::uint8_t
{
    SelectLayout    = 0x00,
    LedLighting     = 0x03,
    Brightness      = 0x08,
    LedSleep        = 0x09,
    LedFeedback     = 0x0A,
    ProgrammerMode  = 0x0E,
    ClearDawState   = 0x12
};

/** Layout identifiers accepted by `SelectLayout`. */
enum class Layout : std::uint8_t
{
    Session     = 0x00,
    Custom1     = 0x04,
    Custom2     = 0x05,
    Custom3     = 0x06,
    DawFaders   = 0x0D,
    Programmer  = 0x7F
};

/**
    One complete SysEx message:

        F0  00 20 29 02 0D  <command>  <payload...>  F7

    All payload bytes must be 7-bit. The Launchpad messages used here carry no
    checksum.
*/
struct SysExFrame
{
    std::vector<std::uint8_t> bytes;

    std::size_t size() const noexcept { return bytes.size(); }

    std::span<const std::uint8_t> data() const noexcept { return bytes; }

    /** True when the envelope, the header and every payload byte are valid. */
    bool isValid() const noexcept;

    bool operator== (const SysExFrame& other) const noexcept
    {
        return bytes == other.bytes;
    }
};

/**
    Builds one frame in the documented envelope.

    Returns an empty frame (and therefore `isValid() == false`) when a payload
    byte is outside 0..127, because such a byte would corrupt the message.
*/
SysExFrame makeSysExFrame (SysExCommand command,
                           std::span<const std::uint8_t> payload = {});

/** `F0 00 20 29 02 0D 0E <0|1> F7`: leave Live mode for programmer mode. */
SysExFrame programmerModeFrame (bool enabled);

/** `F0 00 20 29 02 0D 00 <layout> F7`: select one of the documented layouts. */
SysExFrame selectLayoutFrame (Layout layout);

/** `F0 00 20 29 02 0D 0A <internal> <external> F7`: pad LED feedback. */
SysExFrame ledFeedbackFrame (bool internal, bool external);

/** `F0 00 20 29 02 0D 09 <awake> F7`: wake from, or enter, LED sleep. */
SysExFrame ledSleepFrame (bool awake);

/** True when the frame is a valid Programmer/Live toggle. */
bool isProgrammerModeFrame (std::span<const std::uint8_t> bytes);

//==============================================================================
// Grid
//==============================================================================

inline constexpr int gridRows    = 8;
inline constexpr int gridColumns = 8;
inline constexpr int padCount    = gridRows * gridColumns;

/** Bottom-left grid pad is note 11; top-right is note 88. */
inline constexpr int padNoteFirst = 11;
inline constexpr int padNoteLast  = 88;

/** Note for a pad index, or -1 when the index is out of range. */
constexpr int padNoteForIndex (int index) noexcept
{
    if (index < 0 || index >= padCount)
        return -1;

    const int row    = index / gridColumns + 1;
    const int column = index % gridColumns + 1;

    return 10 * row + column;
}

/**
    Pad index for a grid note, or -1 for any other note.

    A note is a grid note only when both of its decimal digits are 1..8, which
    is exactly the `10 * row + column` addressing the device uses.
*/
constexpr int padIndexForNote (int note) noexcept
{
    const int row    = note / 10;
    const int column = note % 10;

    if (row < 1 || row > gridRows || column < 1 || column > gridColumns)
        return -1;

    return (row - 1) * gridColumns + (column - 1);
}

constexpr bool isGridNote (int note) noexcept
{
    return padIndexForNote (note) >= 0;
}

/** 0-based row from the bottom, or -1. */
constexpr int padRowForIndex (int index) noexcept
{
    return index >= 0 && index < padCount ? index / gridColumns : -1;
}

/** 0-based column from the left, or -1. */
constexpr int padColumnForIndex (int index) noexcept
{
    return index >= 0 && index < padCount ? index % gridColumns : -1;
}

//==============================================================================
// Buttons around the grid (programmer-mode addressing)
//==============================================================================

inline constexpr int topRowCount      = 8;
inline constexpr int rightColumnCount = 8;

/** Top row button CCs are 91..98, left to right. */
constexpr int topRowCCForIndex (int index) noexcept
{
    return index >= 0 && index < topRowCount ? 91 + index : -1;
}

constexpr std::optional<int> topRowIndexForCC (int cc) noexcept
{
    if (cc < 91 || cc > 98)
        return std::nullopt;

    return cc - 91;
}

/** Right column CCs are 89, 79, ... 19 from top to bottom. */
constexpr int rightColumnCCForIndex (int index) noexcept
{
    return index >= 0 && index < rightColumnCount ? 89 - 10 * index : -1;
}

constexpr std::optional<int> rightColumnIndexForCC (int cc) noexcept
{
    if (cc < 19 || cc > 89 || (89 - cc) % 10 != 0)
        return std::nullopt;

    return (89 - cc) / 10;
}

/** The Novation logo LED is CC 99. */
inline constexpr int logoCC = 99;

//==============================================================================
// LED palette
//==============================================================================

/**
    Named colours from the MK3 palette.

    `Green = 21` and `Red = 5` were confirmed on the physical device; the other
    values are the documented palette entries. The value is the velocity byte
    in an LED Note On.
*/
enum class Colour : std::uint8_t
{
    Off     = 0,
    White   = 3,
    Red     = 5,
    Yellow  = 13,
    Green   = 21,
    Blue    = 45,
    Purple  = 49
};

inline constexpr std::array<Colour, 7> palette {
    Colour::Off, Colour::White, Colour::Red, Colour::Yellow,
    Colour::Green, Colour::Blue, Colour::Purple
};

constexpr std::uint8_t velocityFor (Colour colour) noexcept
{
    return static_cast<std::uint8_t> (colour);
}

constexpr bool paletteVelocitiesAreDistinct() noexcept
{
    for (std::size_t i = 0; i < palette.size(); ++i)
        for (std::size_t j = i + 1; j < palette.size(); ++j)
            if (velocityFor (palette[i]) == velocityFor (palette[j]))
                return false;

    return true;
}

static_assert (paletteVelocitiesAreDistinct(),
               "every palette colour must have its own velocity byte");

/** Lighting modes select the MIDI channel of the LED Note On. */
enum class LedMode : std::uint8_t
{
    Static   = 0,
    Flashing = 1,
    Pulsing  = 2
};

/** The three bytes that light one pad: status, grid note, palette velocity. */
constexpr std::array<std::uint8_t, 3> padColourMessage (int padIndex,
                                                        Colour colour,
                                                        LedMode mode = LedMode::Static) noexcept
{
    const int note = padNoteForIndex (padIndex);

    if (note < 0)
        return { 0, 0, 0 };

    return { static_cast<std::uint8_t> (0x90u + static_cast<std::uint8_t> (mode)),
             static_cast<std::uint8_t> (note),
             velocityFor (colour) };
}

/**
    Raw palette velocities used for the 16 patch colours.

    `colourIndex` is the resolved patch palette index in `/state` (0..15). The
    values are distinct MK3 palette velocities: red, orange, yellow, lime,
    green, mint, teal, cyan, sky, azure, blue, violet, purple, magenta, white,
    grey. The armed/warming/error states override these with red/yellow.
*/
inline constexpr std::array<std::uint8_t, 16> patchPaletteVelocities {
    5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 3, 1
};

/** Raw palette velocity for a patch colour index, clamped to 0..15. */
constexpr std::uint8_t velocityForPatchColour (int colourIndex) noexcept
{
    const int index = colourIndex < 0 ? 0
                    : colourIndex > 15 ? 15
                    : colourIndex;

    return patchPaletteVelocities[static_cast<std::size_t> (index)];
}

/** The three bytes that light one pad with a raw palette velocity. */
constexpr std::array<std::uint8_t, 3> padVelocityMessage (
    int padIndex,
    std::uint8_t velocity,
    LedMode mode = LedMode::Static) noexcept
{
    const int note = padNoteForIndex (padIndex);

    if (note < 0)
        return { 0, 0, 0 };

    return { static_cast<std::uint8_t> (0x90u + static_cast<std::uint8_t> (mode)),
             static_cast<std::uint8_t> (note),
             velocity };
}

/** True when `cc` addresses a top-row or right-column button. */
constexpr bool isControlCC (int cc) noexcept
{
    return topRowIndexForCC (cc).has_value()
           || rightColumnIndexForCC (cc).has_value();
}

/** The three bytes that light one top-row or right-column button. */
constexpr std::array<std::uint8_t, 3> buttonColourMessage (
    int cc,
    Colour colour,
    LedMode mode = LedMode::Static) noexcept
{
    if (! isControlCC (cc))
        return { 0, 0, 0 };

    return { static_cast<std::uint8_t> (0xB0u + static_cast<std::uint8_t> (mode)),
             static_cast<std::uint8_t> (cc),
             velocityFor (colour) };
}

} // namespace Launchpad
