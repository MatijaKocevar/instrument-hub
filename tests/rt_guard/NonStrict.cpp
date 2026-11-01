#include "NonStrict.h"
#include "Fixtures.h"

#include "audio/RtGuard.h"

#include <iostream>
#include <string>

namespace RtGuardTest
{

void runNonStrictCountsViolations()
{
    std::cout << "\n-- non-strict mode counts violations without aborting --" << std::endl;

    AudioEngine::RtGuard::setEnabled (true);
    AudioEngine::RtGuard::setStrict (false);

    struct Case
    {
        Injection injection;
        const char* label;
    };

    const Case cases[] = {
        { Injection::allocation, "operator new" },
        { Injection::lock, "pthread_mutex_lock" },
        { Injection::log, "juce::Logger" },
    };

    for (const auto& testCase : cases)
    {
        AudioEngine::RtGuard::resetCounters();

        if (testCase.injection == Injection::log)
            AudioEngine::RtGuard::installLogProbe();

        {
            Harness harness (testCase.injection);
            harness.source.setArmed (true);
            harness.render (3);
        }

        if (testCase.injection == Injection::log)
            AudioEngine::RtGuard::uninstallLogProbe();

        const auto counters = AudioEngine::RtGuard::getCounters();

        switch (testCase.injection)
        {
            case Injection::allocation:
                check (counters.allocations >= 3,
                       std::string ("allocations counted for ") + testCase.label);
                break;
            case Injection::lock:
                check (counters.locks >= 3,
                       std::string ("locks counted for ") + testCase.label);
                break;
            case Injection::log:
                check (counters.logs >= 3,
                       std::string ("logs counted for ") + testCase.label);
                break;
            case Injection::none:
                break;
        }

        std::cout << "  " << toString (testCase.injection) << ": alloc="
                  << counters.allocations << " lock=" << counters.locks
                  << " log=" << counters.logs << " scope="
                  << counters.scopeErrors << std::endl;
    }

    AudioEngine::RtGuard::resetCounters();
}

} // namespace RtGuardTest
