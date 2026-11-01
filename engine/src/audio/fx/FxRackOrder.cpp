#include "audio/FxRack.h"

#include <cstddef>
#include <utility>

namespace AudioEngine
{

//==============================================================================
bool FxRack::reorder (const std::vector<int>& order, std::string& error)
{
    if (order.size() != inserts.size())
    {
        error = "order must list every insert exactly once";
        return false;
    }

    std::vector<bool> seen (inserts.size(), false);
    std::vector<Insert> reordered;
    reordered.reserve (inserts.size());

    for (const int index : order)
    {
        if (index < 0 || index >= size() || seen[static_cast<std::size_t> (index)])
        {
            error = "order must be a permutation of the insert indices";
            return false;
        }

        seen[static_cast<std::size_t> (index)] = true;
        reordered.push_back (std::move (inserts[static_cast<std::size_t> (index)]));
    }

    inserts = std::move (reordered);
    return true;
}

void FxRack::moveInsert (int from, int to) noexcept
{
    if (from == to)
        return;

    Insert moved = std::move (inserts[static_cast<std::size_t> (from)]);
    inserts.erase (inserts.begin() + static_cast<std::ptrdiff_t> (from));
    inserts.insert (inserts.begin() + static_cast<std::ptrdiff_t> (to),
                    std::move (moved));
}

} // namespace AudioEngine
