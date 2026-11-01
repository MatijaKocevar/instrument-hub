#pragma once

#include <string>

namespace ApiTest
{

extern int failures;

void check (bool condition, const std::string& what);

void skip (const std::string& what);

}
