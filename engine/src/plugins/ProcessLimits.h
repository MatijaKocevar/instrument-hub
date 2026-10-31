#pragma once

#include <cstddef>

#if defined(__linux__)
    #include <sys/resource.h>
#endif

namespace ihub::plugins
{

/**
    Caps an absurdly high soft `RLIMIT_NOFILE`.

    yabridge builds a `posix_spawn` file-actions list containing a close() for
    every descriptor below `getdtablesize()`. Some distributions ship a soft
    limit near 2^30, which turns that loop into minutes of `mremap` growth and
    makes a bridged plugin look hung. Lowering a soft limit never needs
    privilege, and the hard limit is left untouched.

    Call this once before the first plugin is loaded (the app and the
    out-of-process scanner both do). It is a no-op when the limit is already
    sane or when the platform is not Linux.
*/
inline void capFileDescriptorLimit (std::size_t maximum = 65536) noexcept
{
   #if defined(__linux__)
    struct rlimit limit
    {
    };

    if (getrlimit (RLIMIT_NOFILE, &limit) != 0)
        return;

    if (limit.rlim_cur == RLIM_INFINITY
        || limit.rlim_cur > static_cast<rlim_t> (maximum))
    {
        auto target = static_cast<rlim_t> (maximum);

        if (limit.rlim_max != RLIM_INFINITY && target > limit.rlim_max)
            target = limit.rlim_max;

        limit.rlim_cur = target;
        setrlimit (RLIMIT_NOFILE, &limit);
    }
   #endif
}

} // namespace ihub::plugins
