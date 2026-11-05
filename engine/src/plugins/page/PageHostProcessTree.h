#pragma once

#include <sys/types.h>

namespace ihub::plugins::page_detail
{

void killProcessTree (pid_t root) noexcept;

} // namespace ihub::plugins::page_detail
