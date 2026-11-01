#pragma once

#include <atomic>

namespace allocCount
{

extern std::atomic<bool> enabled;
extern std::atomic<long> total;

inline void note() noexcept
{
    if (enabled.load (std::memory_order_relaxed))
        total.fetch_add (1, std::memory_order_relaxed);
}

} // namespace allocCount
