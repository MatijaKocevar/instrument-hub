#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/**
    Hardware-free description of the Novation Launchkey Mini MK3 control surface.

    Everything in this file is pure data and pure functions: no MIDI device is
    opened, no message is sent, and no allocation happens on the audio thread
    (nothing here is called from it). The driver, the simulator and the tests all
    share this one description of the protocol, so a byte is only ever written
    down once.

    Sources are recorded in `docs/launchkey-mini-mk3.md`:

    - Novation Launchkey MK3 Programmer's Reference (SysEx header, DAW mode,
      session layout, colour palette, encoder CCs).
    - Novation Launchkey Mini MK3 User Guide (25-key range, 8x2 pad grid,
      pad modes).

    Pad indexing used by Instrument Hub is the device's physical order:
    pads 0..7 are the bottom row left-to-right, pads 8..15 the top row
    left-to-right. The pad note map is the chromatic trigger range required by
    the controller contract: MIDI notes 36..51 map to pads 0..15.
*/
namespace Launchkey
{

//==============================================================================
// Device identity and the SysEx envelope
//==============================================================================

/** Novation EMS registered MIDI manufacturer ID. */
inline constexpr std::array<std::uint8_t, 3> manufacturerId { 0x00, 0x20, 0x29 };

/** Launchkey MK3 product family, as used in the SysEx header. */
inline constexpr std::uint8_t productFamily = 0x02;

/** Launchkey MK3 model byte (the Mini shares the MK3 header). */
inline constexpr std::uint8_t productModel = 0x0F;

/** `F0 00 20 29 02 0F`: every Launchkey MK3 SysEx message starts with this. */
inline constexpr std::array<std::uint8_t, 6> sysExHeader { 0xF0, 0x00, 0x20, 0x29,
                                                            0x02, 0x0F };

inline constexpr std::uint8_t sysExStart = 0xF0;
inline constexpr std::uint8_t sysExEnd   = 0xF7;

/** Command bytes documented by the Launchkey MK3 reference. */
enum class SysExCommand : std::uint8_t
{
    VelocityCurve      = 0x02,
    SetDisplayRow      = 0x04,
    ClearDisplay       = 0x06,
    SetParameterName   = 0x07,
    SetParameterValue  = 0x08,
    StartupAnimation   = 0x78,

    /** Instrument Hub session-mode frame; see `sessionModeFrame()`. */
    SessionMode        = 0x0C
};

/** Whether a frame carries the Instrument Hub trailing checksum. */
enum class Checksum : std::uint8_t
{
    None,
    Xor7
};

/**
    One complete SysEx message.

    Layout without a checksum (the vendor form):

        F0  00 20 29 02 0F  <command>  <payload...>  F7

    Layout with the Instrument Hub extension:

        F0  00 20 29 02 0F  <command>  <payload...>  <checksum>  F7

    All payload bytes are 7-bit. The checksum is the XOR of the command byte and
    every payload byte, masked to 7 bits. It is only added when the caller asks
    for it, so the documented display/velocity/startup messages keep the exact
    vendor byte sequence. The reserved session-mode frame uses it.
*/
struct SysExFrame
{
    std::vector<std::uint8_t> bytes;
    Checksum checksum = Checksum::None;

    std::size_t size() const noexcept { return bytes.size(); }

    std::span<const std::uint8_t> data() const noexcept { return bytes; }

    /** True when the frame is well formed and, if requested, its checksum matches. */
    bool isValid() const noexcept;

    bool operator== (const SysExFrame& other) const noexcept
    {
        return bytes == other.bytes && checksum == other.checksum;
    }
};

/** XOR of the given bytes, masked to 7 bits. */
std::uint8_t checksumOf (std::span<const std::uint8_t> commandAndPayload) noexcept;

/**
    Builds one frame in the documented envelope.

    Returns an empty frame (and therefore `isValid() == false`) if the payload
    contains a byte outside 0..127, because such a byte would terminate or
    corrupt the SysEx message.
*/
SysExFrame makeSysExFrame (SysExCommand command,
                           std::span<const std::uint8_t> payload = {},
                           Checksum checksum = Checksum::None);

/** The session-mode frame documented in `docs/launchkey-mini-mk3.md`.

    Bytes: `F0 00 20 29 02 0F 0C 01 0D F7` (length 10, checksum 0x0D).
*/
SysExFrame sessionModeFrame();

/** True when `bytes` is exactly the documented session-mode frame. */
bool isSessionModeFrame (std::span<const std::uint8_t> bytes);

//==============================================================================
// Session mode
//==============================================================================

/** A three-byte channel message (status, data 1, data 2). */
struct ChannelMessage
{
    std::uint8_t status = 0;
    std::uint8_t data1  = 0;
    std::uint8_t data2  = 0;

    constexpr bool operator== (const ChannelMessage&) const noexcept = default;
};

/** `9F 0C 7F`: enter DAW mode (the prerequisite for session mode). */
inline constexpr ChannelMessage dawModeOn { 0x9F, 0x0C, 0x7F };

/** `9F 0C 00`: leave DAW mode. */
inline constexpr ChannelMessage dawModeOff { 0x9F, 0x0C, 0x00 };

/** `BF 03 02`: select the session layout on the pads. */
inline constexpr ChannelMessage sessionPadLayout { 0xBF, 0x03, 0x02 };

/** `BF 03 01`: select the drum layout on the pads. */
inline constexpr ChannelMessage drumPadLayout { 0xBF, 0x03, 0x01 };

/**
    The working session-mode switch documented by Novation.

    The Launchkey MK3 reference selects DAW mode and the pad layout with notes
    and CCs, not with SysEx. `sessionModeFrame()` is the project's reserved
    SysEx encoding; a driver talking to current firmware must send the channel
    messages in this struct.
*/
struct SessionMode
{
    ChannelMessage enableDawMode  = dawModeOn;
    ChannelMessage selectLayout   = sessionPadLayout;
    ChannelMessage disableDawMode = dawModeOff;
};

inline constexpr SessionMode sessionMode {};

//==============================================================================
// Pads
//==============================================================================

inline constexpr int padCount = 16;

/** MIDI note of pad 0; the pads span 36..51 chromatically. */
inline constexpr int padNoteFirst = 36;
inline constexpr int padNoteLast  = 51;

/** Instrument Hub pad index: 0..7 bottom row, 8..15 top row. */
inline constexpr int padFirstIndex = 0;
inline constexpr int padLastIndex  = padCount - 1;

/** Note for a pad index, or -1 when the index is out of range. */
constexpr int padNoteForIndex (int index) noexcept
{
    return index >= padFirstIndex && index <= padLastIndex ? padNoteFirst + index : -1;
}

/** Pad index for a note in 36..51, or `std::nullopt` for any other note. */
constexpr std::optional<int> padIndexForNote (int note) noexcept
{
    if (note < padNoteFirst || note > padNoteLast)
        return std::nullopt;

    return note - padNoteFirst;
}

constexpr bool isPadNote (int note) noexcept
{
    return note >= padNoteFirst && note <= padNoteLast;
}

/**
    LED note for a pad in the vendor's DAW session layout.

    The MK3 reference addresses the session pads as 0x60..0x67 (top row) and
    0x70..0x77 (bottom row): note 96..103 for pads 8..15 and 112..119 for pads
    0..7. Returns -1 for an out-of-range index.
*/
constexpr int sessionLedNoteForIndex (int index) noexcept
{
    if (index < 0 || index >= padCount)
        return -1;

    return index < 8 ? 112 + index : 96 + (index - 8);
}

//==============================================================================
// LED palette
//==============================================================================

/**
    Named colours from the Launchkey palette.

    The value is the velocity byte sent in a pad Note On. The documented
    examples (red = 0x05, green = 0x13, blue = 0x2D, off = 0x00) are reproduced
    exactly; the remaining names are palette entries from the same 128-colour
    table. Every value here is distinct.
*/
enum class Colour : std::uint8_t
{
    Off        = 0,
    DarkGrey   = 1,
    Grey       = 2,
    White      = 3,
    Pink       = 4,
    Red        = 5,
    Orange     = 9,
    Cream      = 12,
    Green      = 19,
    Mint       = 29,
    LightBlue  = 37,
    Blue       = 45,
    DarkBlue   = 49,
    Violet     = 52,
    Purple     = 53,
    Aqua       = 77,
    Yellow     = 97
};

/** The number of palette slots (velocities 0..127) the device accepts. */
inline constexpr std::size_t paletteSize = 128;

/** Every named colour, in palette order. */
inline constexpr std::array<Colour, 17> palette {
    Colour::Off,       Colour::DarkGrey, Colour::Grey,     Colour::White,
    Colour::Pink,      Colour::Red,      Colour::Orange,   Colour::Cream,
    Colour::Green,     Colour::Mint,     Colour::LightBlue, Colour::Blue,
    Colour::DarkBlue,  Colour::Violet,   Colour::Purple,   Colour::Aqua,
    Colour::Yellow
};

constexpr std::uint8_t velocityFor (Colour colour) noexcept
{
    return static_cast<std::uint8_t> (colour);
}

/** True when no two palette entries share a velocity byte. */
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
    Static   = 1,
    Flashing = 2,
    Pulsing  = 3
};

/** The three bytes that light one pad: status, vendor LED note, palette velocity. */
constexpr std::array<std::uint8_t, 3> padColourMessage (int padIndex,
                                                        Colour colour,
                                                        LedMode mode = LedMode::Static) noexcept
{
    const int note = sessionLedNoteForIndex (padIndex);

    if (note < 0)
        return { 0, 0, 0 };

    return { static_cast<std::uint8_t> (0x90u + static_cast<std::uint8_t> (mode) - 1u),
             static_cast<std::uint8_t> (note),
             velocityFor (colour) };
}

//==============================================================================
// Encoders
//==============================================================================

inline constexpr int encoderCount = 8;

/** Knob 0 is the leftmost; the DAW port reports CC 21..28 on channel 16. */
inline constexpr int encoderCCFirst = 21;
inline constexpr int encoderCCLast  = 28;
inline constexpr int encoderMidiChannel = 16;

constexpr int encoderCCForIndex (int index) noexcept
{
    return index >= 0 && index < encoderCount ? encoderCCFirst + index : -1;
}

constexpr std::optional<int> encoderIndexForCC (int cc) noexcept
{
    if (cc < encoderCCFirst || cc > encoderCCLast)
        return std::nullopt;

    return cc - encoderCCFirst;
}

/** Encoder CC status byte: `BF` (channel 16). */
inline constexpr std::uint8_t encoderStatus = 0xBF;

//==============================================================================
// Keyboard
//==============================================================================

/** The 25 keys span C2..C4 (MIDI 48..72), 25 notes inclusive. */
inline constexpr int keyNoteFirst = 48;
inline constexpr int keyNoteLast  = 72;
inline constexpr int keyCount     = keyNoteLast - keyNoteFirst + 1;

constexpr bool isKeyNote (int note) noexcept
{
    return note >= keyNoteFirst && note <= keyNoteLast;
}

static_assert (keyCount == 25, "the Launchkey Mini MK3 has 25 keys");

} // namespace Launchkey
