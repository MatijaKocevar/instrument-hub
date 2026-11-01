#pragma once

#include <optional>

namespace Launchpad
{

inline constexpr int gridRows    = 8;
inline constexpr int gridColumns = 8;
inline constexpr int padCount    = gridRows * gridColumns;

inline constexpr int padNoteFirst = 11;
inline constexpr int padNoteLast  = 88;

constexpr int padNoteForIndex (int index) noexcept
{
    if (index < 0 || index >= padCount)
        return -1;

    const int row    = index / gridColumns + 1;
    const int column = index % gridColumns + 1;

    return 10 * row + column;
}

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

constexpr int padRowForIndex (int index) noexcept
{
    return index >= 0 && index < padCount ? index / gridColumns : -1;
}

constexpr int padColumnForIndex (int index) noexcept
{
    return index >= 0 && index < padCount ? index % gridColumns : -1;
}

inline constexpr int topRowCount      = 8;
inline constexpr int rightColumnCount = 8;

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

inline constexpr int logoCC = 99;

constexpr bool isControlCC (int cc) noexcept
{
    return topRowIndexForCC (cc).has_value()
           || rightColumnIndexForCC (cc).has_value();
}

}
