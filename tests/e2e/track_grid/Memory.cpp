#include "track_grid/Memory.h"

#include <fstream>
#include <string>

#if defined(__linux__)
 #include <sys/resource.h>
#endif

namespace TrackGridE2E
{

namespace
{

#if defined(__linux__)
bool readStatusValue (const std::string& path, const char* key, long& value)
{
    std::ifstream status (path);
    std::string line;

    while (std::getline (status, line))
    {
        if (line.rfind (key, 0) != 0)
            continue;

        const auto first = line.find_first_of ("0123456789");

        if (first == std::string::npos)
            return false;

        try
        {
            value = std::stol (line.substr (first));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    return false;
}
#endif

}

ProcessMemory readProcessMemory()
{
    ProcessMemory memory;

   #if defined(__linux__)
    (void) readStatusValue ("/proc/self/status", "VmRSS:", memory.currentKb);
    (void) readStatusValue ("/proc/self/status", "VmHWM:", memory.peakKb);

    if (memory.peakKb <= 0)
    {
        rusage usage {};
        getrusage (RUSAGE_SELF, &usage);
        memory.peakKb = usage.ru_maxrss;
    }

    if (memory.currentKb <= 0)
        memory.currentKb = memory.peakKb;
   #endif

    return memory;
}

ProcessMemory readChildProcessMemory (int processId)
{
    ProcessMemory memory;

   #if defined(__linux__)
    if (processId <= 0)
        return memory;

    const auto path = "/proc/" + std::to_string (processId) + "/status";
    (void) readStatusValue (path, "VmRSS:", memory.currentKb);
    (void) readStatusValue (path, "VmHWM:", memory.peakKb);

    if (memory.peakKb <= 0)
        memory.peakKb = memory.currentKb;
   #else
    (void) processId;
   #endif

    return memory;
}

double kilobytesToMegabytes (long kilobytes) noexcept
{
    return static_cast<double> (kilobytes) / 1024.0;
}

}
