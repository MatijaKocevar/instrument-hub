#pragma once

#include <optional>

namespace Launchpad
{

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

/** True when `cc` addresses a top-row or right-column button. */
constexpr bool isControlCC (int cc) noexcept
{
    return topRowIndexForCC (cc).has_value()
           || rightColumnIndexForCC (cc).has_value();
}

} // namespace Launchpad
