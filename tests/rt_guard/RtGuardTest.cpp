#include "Fixtures.h"
#include "CompiledIn.h"
#include "Disabled.h"
#include "NonStrict.h"
#include "NormalPath.h"
#include "Scope.h"
#include "Strict.h"

#include <iostream>

int main()
{
    std::cout << "ihub-rt-guard-test: T014 realtime safety harness" << std::endl;

    RtGuardTest::runGuardIsCompiledIn();
    RtGuardTest::runScopeBookkeeping();
    RtGuardTest::runNormalPath();
    RtGuardTest::runNonStrictCountsViolations();
    RtGuardTest::runStrictAbortsOnInjection();
    RtGuardTest::runDisabledGuardIgnoresViolations();

    std::cout << (RtGuardTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-rt-guard (" << RtGuardTest::failures
              << " failure(s))" << std::endl;

    return RtGuardTest::failures == 0 ? 0 : 1;
}
