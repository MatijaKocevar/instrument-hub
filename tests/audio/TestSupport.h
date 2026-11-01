#pragma once

#include <string>

namespace AudioTest
{

extern int failures;

void check (bool condition, const std::string& what);

bool nearlyEqual (double a, double b, double tolerance);

} // namespace AudioTest
