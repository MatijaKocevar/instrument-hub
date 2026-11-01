#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>

namespace allocCount
{
extern std::atomic<bool> enabled;
extern std::atomic<long> total;

inline void note() noexcept
{
    if (enabled.load (std::memory_order_relaxed))
        total.fetch_add (1, std::memory_order_relaxed);
}
}

namespace SamplerTest
{

extern int failures;

void check (bool condition, const std::string& what);

std::string number (double value, int decimals = 3);

}
