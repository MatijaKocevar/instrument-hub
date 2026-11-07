#pragma once

#include <sys/types.h>

#include <string>
#include <vector>

namespace ihub::plugins::page_detail
{

void killProcessTree (pid_t root) noexcept;
void killProcessesMatching (const char* token) noexcept;
bool hasProcessesMatching (const char* token) noexcept;
void waitForProcessesMatching (const char* token, int timeoutMs) noexcept;
void killProcessesWithWinePrefix (const char* prefix) noexcept;
std::vector<std::string> winePrefixesMatching (const char* token) noexcept;

} // namespace ihub::plugins::page_detail
