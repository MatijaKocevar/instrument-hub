#pragma once

#include <string>

namespace ControllerTest
{

extern int failures;

void check (bool condition, const std::string& what);

} // namespace ControllerTest
