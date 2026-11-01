#include "Strict.h"
#include "Fixtures.h"

#include <iostream>
#include <string>

namespace RtGuardTest
{

void runStrictAbortsOnInjection()
{
    std::cout << "\n-- strict mode aborts on injected violations --" << std::endl;

    struct Case
    {
        Injection injection;
        const char* label;
        const char* needle;
    };

    const Case cases[] = {
        { Injection::allocation, "new inside the callback", "rt-guard: FATAL allocation" },
        { Injection::lock, "std::mutex lock inside the callback", "rt-guard: FATAL lock" },
        { Injection::log, "juce::Logger write inside the callback", "rt-guard: FATAL log" },
    };

    for (const auto& testCase : cases)
    {
        const auto result = runStrictChild (testCase.injection);

        check (result.aborted,
               std::string ("SIGABRT on ") + testCase.label);

        const bool named = result.output.find ("rt-guard: FATAL") != std::string::npos
                           && result.output.find (testCase.needle) != std::string::npos;

        check (named,
               std::string ("diagnostic names the violation for ") + testCase.label);

        if (! result.aborted || ! named)
            std::cout << "  child stderr: " << result.output << std::endl;
    }
}

}
