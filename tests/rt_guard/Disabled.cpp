#include "Disabled.h"
#include "Fixtures.h"

#include "audio/RtGuard.h"

#include <iostream>

namespace RtGuardTest
{

void runDisabledGuardIgnoresViolations()
{
    std::cout << "\n-- a disabled guard ignores violations --" << std::endl;

    AudioEngine::RtGuard::resetCounters();
    AudioEngine::RtGuard::setEnabled (false);
    AudioEngine::RtGuard::setStrict (true);

    {
        Harness harness (Injection::allocation);
        harness.source.setArmed (true);
        harness.render (3);
    }

    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::setEnabled (true);

    check (AudioEngine::RtGuard::getViolationCount() == 0,
           "no violation is recorded while the guard is disabled");
}

}
