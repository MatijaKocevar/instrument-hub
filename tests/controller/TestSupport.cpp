#include "TestSupport.h"

#include <iostream>

namespace ControllerTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

} // namespace ControllerTest
