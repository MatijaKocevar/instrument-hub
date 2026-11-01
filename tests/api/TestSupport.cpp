#include "TestSupport.h"

#include <iostream>

namespace ApiTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

void skip (const std::string& what)
{
    std::cout << "SKIP " << what << std::endl;
}

}
