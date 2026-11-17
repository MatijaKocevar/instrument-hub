#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace ihub::app {

inline constexpr std::array<const char*, 16> patchColours{
    "#fb923c", "#facc15", "#a3e635", "#4ade80", "#34d399", "#2dd4bf", "#22d3ee", "#38bdf8",
    "#60a5fa", "#818cf8", "#a78bfa", "#c084fc", "#e879f9", "#f472b6", "#94a3b8", "#e2e8f0"};

inline int clampColourIndex(int index) noexcept {
    return index < 0 ? 0 : (index > 15 ? 15 : index);
}

inline std::string colourForIndex(int index) {
    return patchColours[static_cast<std::size_t>(clampColourIndex(index))];
}

}  // namespace ihub::app
