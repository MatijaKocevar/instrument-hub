#pragma once

#include <string>

namespace E2E
{

extern int failures;

void check (bool condition, const std::string& what);

} // namespace E2E
