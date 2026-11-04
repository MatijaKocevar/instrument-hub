#pragma once

namespace TrackGridE2E
{

struct ProcessMemory
{
    long currentKb = 0;
    long peakKb = 0;
};

ProcessMemory readProcessMemory();

ProcessMemory readChildProcessMemory (int processId);

double kilobytesToMegabytes (long kilobytes) noexcept;

}
