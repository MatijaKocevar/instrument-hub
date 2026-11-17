#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "LaunchpadGrid.h"

namespace Launchpad {

inline constexpr std::array<std::uint8_t, 3> manufacturerId{0x00, 0x20, 0x29};

inline constexpr std::uint8_t productFamily = 0x02;

inline constexpr std::uint8_t productModel = 0x0D;

inline constexpr const char* deviceName = "Launchpad Mini MK3";

inline constexpr std::array<std::uint8_t, 6> sysExHeader{0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D};

inline constexpr std::uint8_t sysExStart = 0xF0;
inline constexpr std::uint8_t sysExEnd = 0xF7;

enum class SysExCommand : std::uint8_t {
    SelectLayout = 0x00,
    LedLighting = 0x03,
    Brightness = 0x08,
    LedSleep = 0x09,
    LedFeedback = 0x0A,
    ProgrammerMode = 0x0E,
    ClearDawState = 0x12
};

enum class Layout : std::uint8_t {
    Session = 0x00,
    Custom1 = 0x04,
    Custom2 = 0x05,
    Custom3 = 0x06,
    DawFaders = 0x0D,
    Programmer = 0x7F
};

struct SysExFrame {
    std::vector<std::uint8_t> bytes;

    std::size_t size() const noexcept { return bytes.size(); }

    std::span<const std::uint8_t> data() const noexcept { return bytes; }

    bool isValid() const noexcept;

    bool operator==(const SysExFrame& other) const noexcept { return bytes == other.bytes; }
};

SysExFrame makeSysExFrame(SysExCommand command, std::span<const std::uint8_t> payload = {});

SysExFrame programmerModeFrame(bool enabled);

SysExFrame selectLayoutFrame(Layout layout);

SysExFrame ledFeedbackFrame(bool internal, bool external);

SysExFrame ledSleepFrame(bool awake);

SysExFrame rgbPadFrame(int padIndex, std::uint8_t red, std::uint8_t green, std::uint8_t blue);

bool isProgrammerModeFrame(std::span<const std::uint8_t> bytes);

enum class Colour : std::uint8_t {
    Off = 0,
    White = 3,
    Red = 5,
    Yellow = 13,
    Green = 21,
    Blue = 45,
    Purple = 49
};

inline constexpr std::array<Colour, 7> palette{Colour::Off,    Colour::White, Colour::Red,
                                               Colour::Yellow, Colour::Green, Colour::Blue,
                                               Colour::Purple};

constexpr std::uint8_t velocityFor(Colour colour) noexcept {
    return static_cast<std::uint8_t>(colour);
}

constexpr bool paletteVelocitiesAreDistinct() noexcept {
    for (std::size_t i = 0; i < palette.size(); ++i)
        for (std::size_t j = i + 1; j < palette.size(); ++j)
            if (velocityFor(palette[i]) == velocityFor(palette[j])) return false;

    return true;
}

static_assert(paletteVelocitiesAreDistinct(),
              "every palette colour must have its own velocity byte");

enum class LedMode : std::uint8_t { Static = 0, Flashing = 1, Pulsing = 2 };

constexpr std::array<std::uint8_t, 3> padColourMessage(int padIndex, Colour colour,
                                                       LedMode mode = LedMode::Static) noexcept {
    const int note = padNoteForIndex(padIndex);

    if (note < 0) return {0, 0, 0};

    return {static_cast<std::uint8_t>(0x90u + static_cast<std::uint8_t>(mode)),
            static_cast<std::uint8_t>(note), velocityFor(colour)};
}

inline constexpr std::array<std::uint8_t, 16> patchPaletteVelocities{13, 21, 29, 37, 45, 49, 3, 1,
                                                                     17, 25, 33, 41, 48, 8,  2, 12};

constexpr bool patchPaletteAvoidsArmedRed() noexcept {
    for (const auto velocity : patchPaletteVelocities)
        if (velocity == velocityFor(Colour::Red)) return false;

    return true;
}

static_assert(patchPaletteAvoidsArmedRed(), "the patch palette must leave red to the armed patch");

constexpr std::uint8_t velocityForPatchColour(int colourIndex) noexcept {
    const int index = colourIndex < 0 ? 0 : colourIndex > 15 ? 15 : colourIndex;

    return patchPaletteVelocities[static_cast<std::size_t>(index)];
}

inline constexpr std::array<std::uint8_t, 8> trackPaletteVelocities{9, 44, 80, 63, 82, 8, 2, 52};

constexpr std::uint8_t velocityForTrackColour(int trackIndex) noexcept {
    const int index = trackIndex < 0 ? 0 : trackIndex > 7 ? 7 : trackIndex;

    return trackPaletteVelocities[static_cast<std::size_t>(index)];
}

inline constexpr std::array<std::array<std::uint8_t, 3>, 8> trackPaletteRgb{{{{255, 107, 0}},
                                                                             {{106, 0, 255}},
                                                                             {{255, 45, 111}},
                                                                             {{0, 168, 143}},
                                                                             {{255, 0, 200}},
                                                                             {{161, 98, 7}},
                                                                             {{229, 231, 235}},
                                                                             {{126, 34, 206}}}};

constexpr std::array<std::uint8_t, 3> rgbForTrackColour(int trackIndex) noexcept {
    const int index = trackIndex < 0 ? 0 : trackIndex > 7 ? 7 : trackIndex;

    return trackPaletteRgb[static_cast<std::size_t>(index)];
}

constexpr std::array<std::uint8_t, 3> padVelocityMessage(int padIndex, std::uint8_t velocity,
                                                         LedMode mode = LedMode::Static) noexcept {
    const int note = padNoteForIndex(padIndex);

    if (note < 0) return {0, 0, 0};

    return {static_cast<std::uint8_t>(0x90u + static_cast<std::uint8_t>(mode)),
            static_cast<std::uint8_t>(note), velocity};
}

constexpr std::array<std::uint8_t, 3> buttonColourMessage(int cc, Colour colour,
                                                          LedMode mode = LedMode::Static) noexcept {
    if (!isControlCC(cc)) return {0, 0, 0};

    return {static_cast<std::uint8_t>(0xB0u + static_cast<std::uint8_t>(mode)),
            static_cast<std::uint8_t>(cc), velocityFor(colour)};
}

}  // namespace Launchpad
