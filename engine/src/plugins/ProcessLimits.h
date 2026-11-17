#pragma once

#include <cstddef>

#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace ihub::plugins {

inline void capFileDescriptorLimit(std::size_t maximum = 65536) noexcept {
#if defined(__linux__)
    struct rlimit limit{};

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0) return;

    if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > static_cast<rlim_t>(maximum)) {
        auto target = static_cast<rlim_t>(maximum);

        if (limit.rlim_max != RLIM_INFINITY && target > limit.rlim_max) target = limit.rlim_max;

        limit.rlim_cur = target;
        setrlimit(RLIMIT_NOFILE, &limit);
    }
#endif
}

}  // namespace ihub::plugins
