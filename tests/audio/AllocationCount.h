#pragma once

#include <atomic>

namespace allocCount
{

extern std::atomic<bool> enabled;
extern std::atomic<long> total;

}
