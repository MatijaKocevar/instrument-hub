#pragma once

#include <string>

namespace DspTest
{

extern int failures;

void check (bool condition, const std::string& what);

std::string number (double value);

}
