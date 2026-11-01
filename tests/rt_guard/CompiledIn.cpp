#include "CompiledIn.h"
#include "Fixtures.h"

#include "audio/RtGuard.h"

#include <iostream>

namespace RtGuardTest
{

void runGuardIsCompiledIn()
{
    std::cout << "\n-- guard is compiled in --" << std::endl;

    check (AudioEngine::RtGuard::isCompiledIn(),
           "RtGuard::isCompiledIn() reports the harness is active");
    check (AudioEngine::RtGuard::isEnabled(), "guard is enabled by default");
    check (! AudioEngine::RtGuard::isStrict(), "guard is not strict by default");
}

} // namespace RtGuardTest
